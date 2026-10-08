import 'dart:io' show Platform;

import 'package:device_info_plus/device_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../errors/exceptions.dart';

/// BLE 권한 (설계서 3.6절).
/// Android 12+: BLUETOOTH_SCAN(neverForLocation)·CONNECT. Android 11 이하: 위치 권한.
/// iOS: 처음 BLE를 쓸 때 OS가 묻는다 (permission_handler의 iOS 매크로 설정이 필요 없게 여기서는 묻지 않는다).
class BlePermissionService {
  BlePermissionService({DeviceInfoPlugin? info}) : _info = info ?? DeviceInfoPlugin();

  final DeviceInfoPlugin _info;

  /// 거부되면 [BleException] permissionDenied
  Future<void> ensureGranted() async {
    if (!Platform.isAndroid) return;
    final sdk = (await _info.androidInfo).version.sdkInt;
    final permissions =
        sdk >= 31 ? [Permission.bluetoothScan, Permission.bluetoothConnect] : [Permission.locationWhenInUse];
    final result = await permissions.request();
    if (!result.values.every((s) => s.isGranted || s.isLimited)) {
      throw const BleException(BleErrorCode.permissionDenied);
    }
  }

  /// 등록 화면의 기본 휴대폰 이름 (사용자가 고칠 수 있다)
  Future<String> phoneName() async {
    try {
      if (Platform.isAndroid) {
        final a = await _info.androidInfo;
        return '${a.manufacturer} ${a.model}'.trim();
      }
      if (Platform.isIOS) {
        final i = await _info.iosInfo;
        return i.name.isNotEmpty ? i.name : 'iPhone';
      }
    } catch (_) {}
    return 'Phone';
  }
}
