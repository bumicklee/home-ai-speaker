import 'package:equatable/equatable.dart';

/// 메모·일정 종류 (펌웨어 notes 모듈 `kind`)
enum NoteKind {
  memo('memo'),
  event('event');

  const NoteKind(this.id);
  final String id;

  static NoteKind fromId(String? id) => id == 'event' ? event : memo;
}

/// active = 목록, trash = 말로 지움(30일), gone = 지운 기록(id·rev만)
enum NoteState {
  active('active'),
  trash('trash'),
  gone('gone');

  const NoteState(this.id);
  final String id;

  static NoteState fromId(String? id) => values.firstWhere((s) => s.id == id, orElse: () => active);
}

/// 마지막 알림 대화의 결과 (`alert.status`)
enum AlertStatus {
  sent('sent'),
  acked('acked'),
  noAnswer('no_answer'),
  missed('missed');

  const AlertStatus(this.id);
  final String id;

  static AlertStatus? fromId(String? id) => values.where((s) => s.id == id).firstOrNull;
}

/// 일정 시각. 스피커 현지 시각 문자열 그대로 (시간대 변환 없음)
/// `YYYY-MM-DD` = 하루 종일, `YYYY-MM-DDTHH:MM` = 시각 있음
class NoteTime extends Equatable {
  NoteTime.date(DateTime d)
      : at = DateTime(d.year, d.month, d.day),
        allDay = true;

  NoteTime.at(DateTime t)
      : at = DateTime(t.year, t.month, t.day, t.hour, t.minute),
        allDay = false;

  final DateTime at;
  final bool allDay;

  DateTime get day => DateTime(at.year, at.month, at.day);

  static final _re = RegExp(r'^(\d{4})-(\d{2})-(\d{2})(?:[T ](\d{2}):(\d{2}))?');

  /// 형식이 틀리거나 없는 날짜면 null
  static NoteTime? tryParse(Object? s) {
    if (s is! String) return null;
    final m = _re.firstMatch(s.trim());
    if (m == null) return null;
    final y = int.parse(m[1]!), mo = int.parse(m[2]!), d = int.parse(m[3]!);
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return null;
    final date = DateTime(y, mo, d);
    if (date.month != mo) return null; // 2월 30일 같은 날짜
    if (m[4] == null) return NoteTime.date(date);
    final h = int.parse(m[4]!), mi = int.parse(m[5]!);
    if (h > 23 || mi > 59) return null;
    return NoteTime.at(DateTime(y, mo, d, h, mi));
  }

  String format() {
    String two(int v) => v.toString().padLeft(2, '0');
    final date = '${at.year.toString().padLeft(4, '0')}-${two(at.month)}-${two(at.day)}';
    return allDay ? date : '${date}T${two(at.hour)}:${two(at.minute)}';
  }

  @override
  String toString() => format();

  @override
  List<Object?> get props => [at, allDay];
}

/// 시:분 (하루 종일 일정 알림 시각 `all_day_time`)
class ClockTime extends Equatable {
  const ClockTime(this.hour, this.minute);

  final int hour;
  final int minute;

  static const morning = ClockTime(9, 0);

  static ClockTime parse(Object? s, {ClockTime fallback = morning}) {
    if (s is! String) return fallback;
    final m = RegExp(r'^(\d{1,2}):(\d{2})$').firstMatch(s.trim());
    if (m == null) return fallback;
    final h = int.parse(m[1]!), mi = int.parse(m[2]!);
    return h > 23 || mi > 59 ? fallback : ClockTime(h, mi);
  }

  String format() => '${hour.toString().padLeft(2, '0')}:${minute.toString().padLeft(2, '0')}';

  @override
  List<Object?> get props => [hour, minute];
}

/// 마지막 알림 (`alert`). offset = 몇 분 전 알림이었나 (-1 = "나중에 다시" 알림)
class NoteAlert extends Equatable {
  const NoteAlert({required this.offset, required this.status, this.at});

  final int offset;
  final AlertStatus status;
  final DateTime? at;

  @override
  List<Object?> get props => [offset, status, at];
}

/// 메모·일정 하나 (설계서 4절)
class Note extends Equatable {
  const Note({
    required this.id,
    this.kind = NoteKind.memo,
    this.title = '',
    this.body = '',
    this.start,
    this.end,
    this.remind = const [],
    this.reminded = const [],
    this.missed = const [],
    this.snoozeAt,
    this.alert,
    this.created,
    this.updated,
    this.rev = 0,
    this.by = '',
    this.state = NoteState.active,
  });

  final String id;
  final NoteKind kind;
  final String title;
  final String body;
  final NoteTime? start;
  final NoteTime? end;

  /// 시작 몇 분 전에 알릴지 (0 = 정각, 최대 3개)
  final List<int> remind;
  final List<int> reminded;
  final List<int> missed;
  final DateTime? snoozeAt;
  final NoteAlert? alert;
  final DateTime? created;
  final DateTime? updated;
  final int rev;

  /// voice / app
  final String by;
  final NoteState state;

  static const maxReminders = 3;
  static const maxRemindMinutes = 10080;
  static const titleMaxBytes = 80;
  static const bodyMaxBytes = 600;

  bool get isEvent => kind == NoteKind.event;
  bool get isMemo => kind == NoteKind.memo;
  bool get isActive => state == NoteState.active;
  bool get isTrash => state == NoteState.trash;
  bool get isGone => state == NoteState.gone;
  bool get isAllDay => start?.allDay ?? false;

  /// 알림 기준 시각. 하루 종일이면 그날 [allDayTime]
  DateTime? baseTime(ClockTime allDayTime) {
    final s = start;
    if (s == null) return null;
    return s.allDay ? DateTime(s.at.year, s.at.month, s.at.day, allDayTime.hour, allDayTime.minute) : s.at;
  }

  /// [minutes] 전 알림이 울리는 시각
  DateTime? fireTime(int minutes, ClockTime allDayTime) => baseTime(allDayTime)?.subtract(Duration(minutes: minutes));

  /// 아직 울리지 않았고 앞으로 울릴 알림 (분)
  List<int> pendingReminders(DateTime now, ClockTime allDayTime) => [
        for (final m in remind)
          if (!reminded.contains(m) && !missed.contains(m) && (fireTime(m, allDayTime)?.isAfter(now) ?? false)) m,
      ];

  @override
  List<Object?> get props =>
      [id, kind, title, body, start, end, remind, reminded, missed, snoozeAt, alert, created, updated, rev, by, state];
}

/// 만들기·고치기 본문 (id·rev는 스피커가 정한다)
class NoteDraft extends Equatable {
  const NoteDraft({required this.kind, required this.title, this.body = '', this.start, this.end, this.remind});

  final NoteKind kind;
  final String title;
  final String body;
  final NoteTime? start;
  final NoteTime? end;

  /// null = 보내지 않음 (만들 때 스피커 기본 알림을 쓴다)
  final List<int>? remind;

  @override
  List<Object?> get props => [kind, title, body, start, end, remind];
}

/// 일정 알림 설정 (스피커 전체 하나, `/notes/settings`)
class NotesSettings extends Equatable {
  const NotesSettings({
    this.enabled = true,
    this.defaultRemind = const [60],
    this.defaultRemindAllDay = const [0],
    this.allDayTime = ClockTime.morning,
  });

  final bool enabled;
  final List<int> defaultRemind;
  final List<int> defaultRemindAllDay;
  final ClockTime allDayTime;

  NotesSettings copyWith({bool? enabled, List<int>? defaultRemind, List<int>? defaultRemindAllDay, ClockTime? allDayTime}) =>
      NotesSettings(
        enabled: enabled ?? this.enabled,
        defaultRemind: defaultRemind ?? this.defaultRemind,
        defaultRemindAllDay: defaultRemindAllDay ?? this.defaultRemindAllDay,
        allDayTime: allDayTime ?? this.allDayTime,
      );

  @override
  List<Object?> get props => [enabled, defaultRemind, defaultRemindAllDay, allDayTime];
}

/// 폰에 둔 사본 (스피커별 하나)
class NotesCache extends Equatable {
  const NotesCache({this.rev = 0, this.items = const [], this.settings});

  final int rev;

  /// active·trash (gone은 사본에서 지운다)
  final List<Note> items;

  /// 마지막으로 받은 알림 설정 (집 밖에서도 하루 종일 시각 등을 보이게)
  final NotesSettings? settings;

  NotesCache copyWith({int? rev, List<Note>? items, NotesSettings? settings}) =>
      NotesCache(rev: rev ?? this.rev, items: items ?? this.items, settings: settings ?? this.settings);

  /// 바뀐 항목 합치기: id별로 rev가 큰 쪽이 이기고, gone이면 사본에서 지운다.
  /// [replace] = 전체 목록(since=0)이라 받은 것에 없는 항목은 지운다
  NotesCache merge(int serverRev, Iterable<Note> changes, {bool replace = false}) {
    final byId = <String, Note>{if (!replace) for (final n in items) n.id: n};
    for (final c in changes) {
      final old = byId[c.id];
      if (old != null && old.rev > c.rev) continue;
      if (c.isGone) {
        byId.remove(c.id);
      } else {
        byId[c.id] = c;
      }
    }
    final next = replace ? serverRev : (serverRev > rev ? serverRev : rev);
    return NotesCache(rev: next, items: byId.values.toList(), settings: settings);
  }

  @override
  List<Object?> get props => [rev, items, settings];
}
