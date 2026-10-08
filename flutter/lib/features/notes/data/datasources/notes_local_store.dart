import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import '../../domain/entities/note.dart';
import '../models/note_model.dart';

/// 폰에 두는 사본: 앱 문서 폴더의 `notes_<deviceId>.json` (`{rev, items, settings}`)
class NotesLocalStore {
  NotesLocalStore({Future<Directory> Function()? directory}) : _directory = directory ?? getApplicationDocumentsDirectory;

  final Future<Directory> Function() _directory;

  Future<File> _file(String deviceId) async {
    final safe = deviceId.replaceAll(RegExp(r'[^A-Za-z0-9_-]'), '_');
    return File('${(await _directory()).path}/notes_$safe.json');
  }

  /// 없거나 깨졌으면 빈 사본
  Future<NotesCache> read(String deviceId) async {
    try {
      final f = await _file(deviceId);
      if (!await f.exists()) return const NotesCache();
      final j = jsonDecode(await f.readAsString());
      return j is Map<String, dynamic> ? NotesCacheModel.fromJson(j) : const NotesCache();
    } on Exception {
      return const NotesCache();
    }
  }

  /// 임시 파일에 쓰고 이름을 바꾼다 (쓰다 앱이 꺼져도 이전 사본이 남게)
  Future<void> write(String deviceId, NotesCache cache) async {
    final f = await _file(deviceId);
    final tmp = File('${f.path}.tmp');
    await tmp.writeAsString(jsonEncode(NotesCacheModel.toJson(cache)), flush: true);
    await tmp.rename(f.path);
  }
}
