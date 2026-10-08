/// 스피커와의 BLE 작업 (설계서 2.13, 3.4절). 첫 설정·텔레그램 토큰·휴대폰 추가가 모두 이것을 쓴다.
/// 실제 구현은 BleLink·BleSecretWriter, 테스트는 가짜.
library;

import 'dart:async';

import '../constants/ble_protocol.dart';
import '../errors/exceptions.dart';
import '../services/ble_permission_service.dart';
import 'ble_link.dart';
import 'ble_secret_writer.dart';
import 'wifi_scan.dart';

/// 연결된 스피커 하나
abstract interface class BleSpeakerSession {
  /// 기기 식별자 (0xFF61 STA MAC)
  String get mac;
  Future<DeviceInfo> deviceInfo();
  Future<List<WifiNetwork>> scanWifi();
  Future<WifiProvStatus> provisionWifi(String ssid, String password);
  Future<void> setSecret(String key, String value);
  Future<void> eraseSecret(String key);
  Future<PairResult> pair({required String phoneId, required String phoneName, String? evict});

  /// `settings.set` (부분 수정). 키 없는 AI를 고르면 [BleCommandException] `NO_API_KEY`
  Future<void> setSettings(String group, Map<String, Object?> value);

  /// 0xFF88: Wi-Fi를 바꿨으면 저장 후 재부팅, 아니면 BLE 창만 닫는다
  Future<void> finish();

  /// prov.close: 쓴 값 적용, BLE 창 닫기, 재부팅 없음 (첫 설정이 아닌 작업의 끝)
  Future<void> closeWindow();
  Future<void> disconnect();
}

abstract interface class BleSpeakerDataSource {
  /// 권한 + Bluetooth 켜짐 확인
  Future<void> ensureReady();
  Stream<List<DiscoveredSpeaker>> scan();
  Future<BleSpeakerSession> connect(DiscoveredSpeaker speaker);

  /// 이미 등록한 스피커(기기 ID = STA MAC)를 찾아 연결한다. 사용자가 Key2를 3초 눌러 BLE 창을 연 뒤에 부른다.
  /// [timeout] 안에 광고가 안 보이면 [BleException] notFound
  Future<BleSpeakerSession> connectById(String deviceId, {Duration timeout});
  Future<String> defaultPhoneName();
}

class BleSpeakerDataSourceImpl implements BleSpeakerDataSource {
  BleSpeakerDataSourceImpl(this._permissions);

  final BlePermissionService _permissions;

  @override
  Future<void> ensureReady() async {
    await _permissions.ensureGranted();
    await BleScanner.ensureOn();
  }

  @override
  Stream<List<DiscoveredSpeaker>> scan() => BleScanner.scan();

  @override
  Future<BleSpeakerSession> connect(DiscoveredSpeaker speaker) async =>
      _BleSession(await BleLink.connect(speaker.device));

  @override
  Future<BleSpeakerSession> connectById(String deviceId, {Duration timeout = const Duration(seconds: 20)}) async {
    await ensureReady();
    final shortId = deviceId.substring(deviceId.length - 6).toUpperCase();
    DiscoveredSpeaker? target;
    await for (final list in BleScanner.scan(timeout: timeout)) {
      target = list.where((s) => s.shortId.toUpperCase() == shortId).firstOrNull;
      if (target != null) break;
    }
    if (target == null) throw const BleException(BleErrorCode.notFound);
    final session = await connect(target);
    if (session.mac.toUpperCase() != deviceId.toUpperCase()) {
      await session.disconnect();
      throw const BleException(BleErrorCode.notFound, 'another speaker with the same short id');
    }
    return session;
  }

  @override
  Future<String> defaultPhoneName() => _permissions.phoneName();
}

class _BleSession implements BleSpeakerSession {
  _BleSession(this._link) : _writer = BleSecretWriter(_link.commands);

  final BleLink _link;
  final BleSecretWriter _writer;

  @override
  String get mac => _link.mac;

  @override
  Future<DeviceInfo> deviceInfo() => _writer.deviceInfo();

  @override
  Future<List<WifiNetwork>> scanWifi() => _link.scanWifi();

  @override
  Future<WifiProvStatus> provisionWifi(String ssid, String password) => _link.provisionWifi(ssid, password);

  @override
  Future<void> setSecret(String key, String value) => _writer.setSecret(key, value);

  @override
  Future<void> eraseSecret(String key) => _writer.eraseSecret(key);

  @override
  Future<void> closeWindow() async {
    try {
      await _writer.close();
    } on BleException catch (e) {
      // 닫으면서 기기가 연결을 끊어 응답이 안 올 수 있다
      if (e.code != BleErrorCode.disconnected && e.code != BleErrorCode.timeout) rethrow;
    }
  }

  @override
  Future<PairResult> pair({required String phoneId, required String phoneName, String? evict}) =>
      _writer.pair(phoneId: phoneId, phoneName: phoneName, evict: evict);

  @override
  Future<void> setSettings(String group, Map<String, Object?> value) => _writer.setSettings(group, value);

  @override
  Future<void> finish() async {
    try {
      await _link.provisioningDone();
    } catch (_) {
      // 기기가 바로 재부팅하며 연결이 끊겨 쓰기 응답이 안 올 수 있다. 저장은 기기가 이미 했다
    }
  }

  @override
  Future<void> disconnect() => _link.disconnect();
}
