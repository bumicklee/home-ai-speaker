import 'package:dartz/dartz.dart';

import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../domain/entities/audio_settings.dart';
import '../models/audio_settings_model.dart';

/// 오디오·호출어·LED 설정 (설계서 2.14절). 선택된 기기로 보낸다.
/// PUT은 바뀐 키만 보낸다 (기기는 없는 키를 그대로 두고, 저장 후 그룹 전체를 돌려준다)
class AudioRepository {
  AudioRepository(this._device);

  final CurrentDevice _device;

  Future<Either<Failure, AudioSettings>> audio() =>
      guard(() async => AudioSettingsModel.audioFromJson(await _get(SettingsGroup.audio)));

  Future<Either<Failure, WakeSettings>> wake() =>
      guard(() async => AudioSettingsModel.wakeFromJson(await _get(SettingsGroup.wake)));

  Future<Either<Failure, int>> ledBrightness() =>
      guard(() async => AudioSettingsModel.ledFromJson(await _get(SettingsGroup.led)));

  /// 키: volume, mic_gain, auto_gain, duck_db
  Future<Either<Failure, AudioSettings>> updateAudio(Map<String, Object> change) =>
      guard(() async => AudioSettingsModel.audioFromJson(await _put(SettingsGroup.audio, change)));

  /// 키: enabled, phrase(0/1), sensitivity, gain_db
  Future<Either<Failure, WakeSettings>> updateWake(Map<String, Object> change) =>
      guard(() async => AudioSettingsModel.wakeFromJson(await _put(SettingsGroup.wake, change)));

  Future<Either<Failure, int>> setLedBrightness(int brightness) =>
      guard(() async => AudioSettingsModel.ledFromJson(await _put(SettingsGroup.led, {'brightness': brightness})));

  Future<Either<Failure, Unit>> testTone() => _post(ApiPaths.testTone);

  /// 60초 감지 테스트 시작. 감지될 때마다 WS `wake_test`. 대화 중이면 409 VOICE_ACTIVE
  Future<Either<Failure, Unit>> startWakeTest() => _post(ApiPaths.wakeTest);

  Future<Map<String, dynamic>> _get(String group) async =>
      await _device.client.get<Map<String, dynamic>>(ApiPaths.settings(group));

  Future<Map<String, dynamic>> _put(String group, Map<String, Object> change) async =>
      await _device.client.put<Map<String, dynamic>>(ApiPaths.settings(group), change);

  Future<Either<Failure, Unit>> _post(String path) => guard(() async {
        await _device.client.post<dynamic>(path);
        return unit;
      });
}
