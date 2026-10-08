import 'package:dartz/dartz.dart';

import '../../../../core/ble/ble_link.dart';
import '../../../../core/ble/ble_secret_writer.dart';
import '../../../../core/ble/wifi_scan.dart';
import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/errors/failures.dart';
import '../../../devices/domain/entities/device.dart';

abstract interface class ProvisioningRepository {
  Future<Either<Failure, Unit>> ensureReady();
  Stream<List<DiscoveredSpeaker>> scan();
  Future<String> defaultPhoneName();

  /// 이 설치의 휴대폰 ID (api.pair)
  String get phoneId;

  Future<Either<Failure, DeviceInfo>> connect(DiscoveredSpeaker speaker);
  Future<Either<Failure, List<WifiNetwork>>> scanWifi();
  Future<Either<Failure, WifiProvStatus>> provisionWifi(String ssid, String password);
  Future<Either<Failure, Unit>> setSecret(String key, String value);
  Future<Either<Failure, PairResult>> pair(String phoneName, {String? evict});

  /// BLE `settings.set` (등록 뒤 고른 AI·방식). 키가 없으면 BleFailure code `NO_API_KEY`
  Future<Either<Failure, Unit>> setSettings(String group, Map<String, Object?> value);
  Future<Either<Failure, Unit>> finish();
  Future<void> disconnect();

  /// 재부팅 뒤 mDNS로 다시 찾는다. 못 찾으면 host 없이 돌려준다
  Future<Device> discover(String mac, String name, {Duration timeout});

  /// 휴대폰에 저장 (토큰은 보안 저장소)
  Future<Either<Failure, Unit>> save(Device device, String token);
}
