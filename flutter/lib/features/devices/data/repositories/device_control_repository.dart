import 'package:dartz/dartz.dart';

import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';

/// 홈 화면의 즉시 동작 (대화 시작·끝, 볼륨, 재생 제어). 선택된 기기로 보낸다
class DeviceControlRepository {
  DeviceControlRepository(this._device);

  final CurrentDevice _device;

  Future<Either<Failure, Unit>> startVoice() => _post(ApiPaths.voice('start'));
  Future<Either<Failure, Unit>> stopVoice() => _post(ApiPaths.voice('stop'));

  /// pause / resume / stop / next / previous
  Future<Either<Failure, Unit>> media(String action) => _post(ApiPaths.mediaControl(action));

  /// 0–80. 기기가 저장하고 WS `volume`으로 알린다
  Future<Either<Failure, Unit>> setVolume(int volume) => guard(() async {
        await _device.client.put<dynamic>(ApiPaths.settings(SettingsGroup.audio), {'volume': volume});
        return unit;
      });

  Future<Either<Failure, Unit>> _post(String path) => guard(() async {
        await _device.client.post<dynamic>(path);
        return unit;
      });
}
