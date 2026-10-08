import 'package:dartz/dartz.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../../../../core/constants/api_paths.dart';
import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/device/current_device.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../../../core/services/device_event_service.dart';
import '../../domain/entities/lesson.dart';
import '../models/lesson_model.dart';

/// 영어 선생님 (펌웨어 lesson.c, LAN /api/v1/lesson)
class LessonRepository {
  LessonRepository(this._device, this._events, this._prefs);

  final CurrentDevice _device;
  final DeviceEventService _events;
  final SharedPreferences _prefs;

  static const _path = '/api/v1/lesson';
  static const _prefsKey = 'english_options';

  Future<Either<Failure, Lesson>> current() => guard(() async {
        final j = await _device.client.get<Map<String, dynamic>>(_path);
        return LessonModel.fromJson(j);
      });

  /// 스피커에 OpenAI 키가 있는지 (`/status.secrets`). 영어 수업은 늘 OpenAI 실시간이라 꼭 필요하다
  Future<Either<Failure, bool>> openAiKeySaved() => guard(() async {
        final j = await _device.client.get<Map<String, dynamic>>(ApiPaths.status);
        return (j['secrets'] as Map<String, dynamic>? ?? const {})[SecretKey.openai] == true;
      });

  /// 대화 중이면 409 VOICE_ACTIVE, OpenAI 키가 없으면 409 NO_API_KEY (AI 설정과 상관없이), Wi-Fi 없으면 409
  Future<Either<Failure, Lesson>> start(LessonOptions options) => guard(() async {
        final j = await _device.client.post<Map<String, dynamic>>('$_path/start', options.toJson());
        return LessonModel.fromJson(j);
      });

  Future<Either<Failure, Unit>> stop() => guard(() async {
        await _device.client.post<dynamic>('$_path/stop');
        return unit;
      });

  Future<Either<Failure, Unit>> changeTopic() => guard(() async {
        await _device.client.post<dynamic>('$_path/topic');
        return unit;
      });

  /// WS `lesson` (수업 상태·고친 표현이 바뀔 때마다 전체)
  Stream<Lesson> updates() => _events.on('lesson').map((e) => LessonModel.fromJson(e.data));

  /// 마지막으로 고른 코스·옵션 (이 휴대폰)
  LessonOptions savedOptions() {
    final raw = _prefs.getStringList(_prefsKey);
    if (raw == null || raw.length != 4) return const LessonOptions();
    final minutes = int.tryParse(raw[3]) ?? 5;
    return LessonOptions(
      course: LessonCourse.fromId(raw[0]),
      explainKorean: raw[1] == 'ko',
      slow: raw[2] == 'slow',
      minutes: LessonOptions.allowedMinutes.contains(minutes) ? minutes : 5,
    );
  }

  Future<void> saveOptions(LessonOptions o) => _prefs.setStringList(
      _prefsKey, [o.course.id, o.explainKorean ? 'ko' : 'en', o.slow ? 'slow' : 'normal', '${o.minutes}']);
}
