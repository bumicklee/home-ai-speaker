import '../../../../l10n/app_localizations.dart';
import '../../domain/entities/lesson.dart';

/// 코스 이름·설명·수준 (디자인 순서: 발음 → 기초 → 생활 → 비즈니스 → 고급)
extension LessonCourseText on LessonCourse {
  String title(AppLocalizations l) => switch (this) {
        LessonCourse.pron => l.engCoursePron,
        LessonCourse.basic => l.engCourseBasic,
        LessonCourse.daily => l.engCourseDaily,
        LessonCourse.biz => l.engCourseBiz,
        LessonCourse.adv => l.engCourseAdv,
      };

  String description(AppLocalizations l) => switch (this) {
        LessonCourse.pron => l.engCoursePronDesc,
        LessonCourse.basic => l.engCourseBasicDesc,
        LessonCourse.daily => l.engCourseDailyDesc,
        LessonCourse.biz => l.engCourseBizDesc,
        LessonCourse.adv => l.engCourseAdvDesc,
      };

  String level(AppLocalizations l) => switch (this) {
        LessonCourse.pron => l.engLevelAll,
        LessonCourse.basic => l.engLevelBeginner,
        LessonCourse.daily => l.engLevelEasy,
        LessonCourse.biz => l.engLevelMid,
        LessonCourse.adv => l.engLevelAdvanced,
      };

  /// 아이콘 자리의 한 글자 (디자인). 한글 음절은 한 글자 = UTF-16 한 단위
  String badge(AppLocalizations l) => title(l).substring(0, 1);
}

String formatRemaining(Duration d) {
  final m = d.inMinutes;
  final s = d.inSeconds % 60;
  return '$m:${s.toString().padLeft(2, '0')}';
}
