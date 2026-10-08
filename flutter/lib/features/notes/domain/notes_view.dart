/// 목록 화면용 계산 (묶기·정렬·찾기·알림 상태). 위젯 없이 테스트한다
library;

import 'entities/note.dart';

/// 일정 묶음 (디자인 "메모·일정": 오늘 · 내일 · 이번 주 · 나중 · 지난 일정)
enum EventGroupKind { today, tomorrow, thisWeek, later, past }

class EventGroup {
  const EventGroup(this.kind, this.day, this.items);

  final EventGroupKind kind;

  /// 오늘·내일 묶음의 날짜 (제목 "오늘 · 10월 4일 (일)")
  final DateTime day;
  final List<Note> items;
}

/// 목록 줄 오른쪽 상태 표시
enum NoteBadge { none, scheduled, sent, acked, noAnswer, missed }

DateTime _day(DateTime t) => DateTime(t.year, t.month, t.day);

/// 제목·내용에 글자가 들어 있나 (대소문자·띄어쓰기 무시, 스피커 notes_search와 같은 방식)
bool matchesQuery(Note n, String query) {
  String norm(String s) => s.toLowerCase().replaceAll(RegExp(r'\s+'), '');
  final q = norm(query);
  if (q.isEmpty) return true;
  return norm(n.title).contains(q) || norm(n.body).contains(q);
}

int _byStart(Note a, Note b) {
  final sa = a.start, sb = b.start;
  if (sa == null || sb == null) return sa == null ? (sb == null ? 0 : 1) : -1;
  final c = sa.day.compareTo(sb.day);
  if (c != 0) return c;
  if (sa.allDay != sb.allDay) return sa.allDay ? -1 : 1; // 같은 날은 하루 종일이 먼저
  return sa.at.compareTo(sb.at);
}

/// 일정을 날짜로 묶는다. 이번 주 = 모레부터 7일 안, 지난 일정은 최근 것부터
List<EventGroup> groupEvents(Iterable<Note> notes, DateTime now) {
  final today = _day(now);
  final tomorrow = today.add(const Duration(days: 1));
  final weekEnd = today.add(const Duration(days: 7));
  final buckets = {for (final k in EventGroupKind.values) k: <Note>[]};
  for (final n in notes) {
    if (!n.isActive || !n.isEvent || n.start == null) continue;
    final d = n.start!.day;
    final kind = d.isBefore(today)
        ? EventGroupKind.past
        : d == today
            ? EventGroupKind.today
            : d == tomorrow
                ? EventGroupKind.tomorrow
                : d.isBefore(weekEnd)
                    ? EventGroupKind.thisWeek
                    : EventGroupKind.later;
    buckets[kind]!.add(n);
  }
  for (final e in buckets.entries) {
    e.value.sort(_byStart);
    if (e.key == EventGroupKind.past) e.value.setAll(0, e.value.reversed.toList());
  }
  return [
    for (final k in EventGroupKind.values)
      if (buckets[k]!.isNotEmpty) EventGroup(k, k == EventGroupKind.tomorrow ? tomorrow : today, buckets[k]!),
  ];
}

/// 메모: 최근에 고친 것부터
List<Note> sortedMemos(Iterable<Note> notes) =>
    notes.where((n) => n.isActive && n.isMemo).toList()..sort((a, b) => _updated(b).compareTo(_updated(a)));

/// 휴지통: 최근에 지운 것부터
List<Note> sortedTrash(Iterable<Note> notes) =>
    notes.where((n) => n.isTrash).toList()..sort((a, b) => _updated(b).compareTo(_updated(a)));

DateTime _updated(Note n) => n.updated ?? n.created ?? DateTime.fromMillisecondsSinceEpoch(0);

/// 휴지통에서 완전히 지워지기까지 남은 날 (스피커가 30일 뒤 gone으로 바꾼다)
int trashDaysLeft(Note n, DateTime now, {int keepDays = 30}) {
  final since = n.updated;
  if (since == null) return keepDays;
  final left = keepDays - now.difference(since).inDays;
  return left.clamp(0, keepDays);
}

/// 알림이 방금 끝난 것으로 보는 시간 (이 안이면 다음 알림이 남아 있어도 결과를 보여 준다)
const alertFreshFor = Duration(hours: 6);

/// 상태 표시: 최근 알림 결과가 있으면 그것, 앞으로 울릴 알림이 있으면 "알림 예정", 놓친 것만 있으면 "놓침"
NoteBadge badgeOf(Note n, DateTime now, ClockTime allDayTime) {
  if (!n.isEvent || !n.isActive) return NoteBadge.none;
  final pending = n.pendingReminders(now, allDayTime).isNotEmpty || (n.snoozeAt?.isAfter(now) ?? false);
  final alert = n.alert;
  if (alert != null) {
    final at = alert.at;
    final fresh = at == null || now.difference(at) < alertFreshFor;
    if (!pending || fresh) {
      return switch (alert.status) {
        AlertStatus.sent => NoteBadge.sent,
        AlertStatus.acked => NoteBadge.acked,
        AlertStatus.noAnswer => NoteBadge.noAnswer,
        AlertStatus.missed => NoteBadge.missed,
      };
    }
  }
  if (pending) return NoteBadge.scheduled;
  if (n.missed.isNotEmpty) return NoteBadge.missed;
  return NoteBadge.none;
}

/// 홈 타일 부제 ("오늘 2개 · 3시 병원")
class TodaySummary {
  const TodaySummary(this.count, this.next);

  final int count;

  /// 오늘 아직 지나지 않은 다음 일정 (없으면 오늘 첫 일정, 오늘 일정이 없으면 null)
  final Note? next;
}

TodaySummary todaySummary(Iterable<Note> notes, DateTime now) {
  final today = groupEvents(notes, now).where((g) => g.kind == EventGroupKind.today).firstOrNull?.items ?? const [];
  final upcoming = today.where((n) => !n.isAllDay && !n.start!.at.isBefore(now)).firstOrNull;
  return TodaySummary(today.length, upcoming ?? today.firstOrNull);
}

/// 알림 칩 고르기: 이미 있으면 빼고, 없으면 최대 3개까지 넣는다. 큰 값(이른 알림)부터 정렬
List<int> toggleRemind(List<int> current, int minutes) {
  if (current.contains(minutes)) return [...current]..remove(minutes);
  if (current.length >= Note.maxReminders || minutes < 0 || minutes > Note.maxRemindMinutes) return current;
  return [...current, minutes]..sort((a, b) => b.compareTo(a));
}
