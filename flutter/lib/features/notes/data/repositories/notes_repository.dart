import 'dart:async';

import 'package:dartz/dartz.dart';

import '../../../../core/device/current_device.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../../../core/services/device_event_service.dart';
import '../../domain/entities/note.dart';
import '../datasources/notes_local_store.dart';
import '../datasources/notes_remote_datasource.dart';
import '../models/note_model.dart';

/// 메모·일정 (원본 = 스피커, 사본 = 이 폰). 설계서 docs/design-notes-schedule.md 6.2절
///
/// 동기화: `GET ?since=<사본 rev>` → id별로 rev 큰 쪽 반영, gone은 사본에서 지움 → 파일에 저장.
/// 스피커 rev가 사본보다 작으면(스피커 초기화) 전체(since=0)를 다시 받아 바꾼다.
class NotesRepository {
  NotesRepository(this._device, this._events, this._remote, this._store);

  final CurrentDevice _device;
  final DeviceEventService _events;
  final NotesRemoteDataSource _remote;
  final NotesLocalStore _store;

  NotesCache _cache = const NotesCache();
  String? _cacheFor;

  static const eventNotes = 'notes';
  static const eventReminder = 'reminder';

  /// 이 폰에 있는 사본 (스피커에 닿지 않아도 된다)
  Future<NotesCache> cached() async {
    final id = _device.deviceId;
    if (id == null) return const NotesCache();
    if (_cacheFor != id) {
      _cache = await _store.read(id);
      _cacheFor = id;
    }
    return _cache;
  }

  Future<Either<Failure, NotesCache>> sync() => guard(() async {
        final base = await cached();
        var j = await _remote.changes(base.rev);
        var full = base.rev == 0;
        if (_rev(j) < base.rev) {
          j = await _remote.changes(0);
          full = true;
        }
        return _save(base.merge(_rev(j), _items(j), replace: full));
      });

  /// remind가 null이면 스피커 기본 알림
  Future<Either<Failure, NotesCache>> create(NoteDraft draft) => guard(() async {
        final j = await _remote.create(NoteModel.createJson(draft));
        return _apply(NoteModel.fromJson(j));
      });

  /// 409 CONFLICT면 본문의 최신 항목으로 사본을 바꾸고 실패(errorDeviceConflict)를 돌려준다
  Future<Either<Failure, NotesCache>> update(Note note, NoteDraft draft) => guard(() async {
        try {
          final j = await _remote.update(note.id, NoteModel.updateJson(draft, note.rev));
          return await _apply(NoteModel.fromJson(j));
        } on DeviceApiException catch (e) {
          final latest = e.data?['note'];
          if (e.code == 'CONFLICT' && latest is Map<String, dynamic>) await _apply(NoteModel.fromJson(latest));
          rethrow;
        }
      });

  /// 스피커 원본 삭제 (되살릴 수 없다)
  Future<Either<Failure, NotesCache>> delete(Note note) => guard(() async {
        final j = await _remote.delete(note.id);
        final rev = _rev(j);
        return _apply(Note(id: note.id, rev: rev > note.rev ? rev : note.rev, state: NoteState.gone));
      });

  Future<Either<Failure, NotesCache>> restore(Note note) => guard(() async {
        final j = await _remote.restore(note.id);
        return _apply(NoteModel.fromJson(j));
      });

  Future<Either<Failure, NotesSettings>> settings() => guard(() async {
        final s = NotesSettingsModel.fromJson(await _remote.settings());
        await _save((await cached()).copyWith(settings: s));
        return s;
      });

  /// 바뀐 키만 보낸다
  Future<Either<Failure, NotesSettings>> saveSettings(NotesSettings from, NotesSettings to) => guard(() async {
        final body = NotesSettingsModel.diff(from, to);
        if (body.isEmpty) return to;
        final j = await _remote.saveSettings(body);
        final s = j.isEmpty ? to : NotesSettingsModel.fromJson(j);
        await _save((await cached()).copyWith(settings: s));
        return s;
      });

  /// 다시 가져올 때: WS `notes`(내용 없이 rev만), `reminder`(알림 결과), 이벤트 연결이 (다시) 열림
  Stream<void> changes() {
    final out = StreamController<void>();
    final subs = <StreamSubscription<Object?>>[];
    out
      ..onListen = () {
        subs
          ..add(_events.events
              .where((e) => e.type == eventNotes || e.type == eventReminder)
              .listen((_) => out.add(null)))
          ..add(_events.connection.where((c) => c == EventConnection.connected).listen((_) => out.add(null)));
      }
      ..onCancel = () async {
        for (final s in subs) {
          await s.cancel();
        }
      };
    return out.stream;
  }

  /// 항목 하나 반영. 사본 rev는 올리지 않는다 (그 사이 다른 변경을 다음 sync에서 놓치지 않게)
  Future<NotesCache> _apply(Note n) async {
    final base = await cached();
    return _save(base.merge(base.rev, [n]));
  }

  Future<NotesCache> _save(NotesCache c) async {
    final id = _device.deviceId;
    if (id == null) return c;
    _cache = c;
    _cacheFor = id;
    try {
      await _store.write(id, c);
    } on Exception {
      // 파일을 못 써도 화면은 메모리 사본으로 계속 (다음 저장 때 다시 쓴다)
    }
    return c;
  }

  static int _rev(Map<String, dynamic> j) => (j['rev'] as num?)?.toInt() ?? 0;

  static List<Note> _items(Map<String, dynamic> j) => [
        for (final i in (j['items'] as List<dynamic>? ?? const []))
          if (i is Map<String, dynamic>) NoteModel.fromJson(i),
      ].where((n) => n.id.isNotEmpty).toList();
}
