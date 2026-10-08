/// 비밀값·등록 명령 공통 처리. provisioning·keys·telegram·spotify가 모두 이것만 쓴다
/// (기능마다 BLE 코드를 중복하지 않는다. 설계서 3.2절). 비밀값은 LAN으로 보내지 않는다 (D6).
library;

import '../ai/ai_choice.dart';
import '../constants/ble_protocol.dart';
import 'ble_command_channel.dart';

class DeviceInfo {
  const DeviceInfo({
    required this.protocolVersion,
    required this.firmware,
    required this.mac,
    required this.provisioned,
    required this.wifiConnected,
    required this.secrets,
    this.ai = AiStatus.legacy,
  });

  factory DeviceInfo.fromJson(Map<String, dynamic> j) => DeviceInfo(
        protocolVersion: j['protocol_version'] as int? ?? 0,
        firmware: j['fw'] as String? ?? '',
        mac: j['mac'] as String? ?? '',
        provisioned: j['provisioned'] as bool? ?? false,
        wifiConnected: j['wifi_connected'] as bool? ?? false,
        secrets: {
          for (final e in (j['secrets'] as Map<String, dynamic>? ?? const {}).entries) e.key: e.value == true,
        },
        // 옛 펌웨어(ai 없음)는 openai.live만
        ai: AiStatus.fromJson(j['ai']),
      );

  final int protocolVersion;
  final String firmware;
  final String mac;
  final bool provisioned;
  final bool wifiConnected;
  final Map<String, bool> secrets;

  /// 지금 쓰는 AI와 이 펌웨어가 할 수 있는 조합 (`/status.ai`와 같은 모양)
  final AiStatus ai;

  /// 펌웨어 규격이 앱보다 새로우면 앱 업데이트가 필요하다
  bool get needsAppUpdate => protocolVersion > BleProtocol.protocolVersion;
}

class PairResult {
  const PairResult({required this.token, required this.phoneId});
  final String token;
  final String phoneId;
}

class BleSecretWriter {
  BleSecretWriter(this._channel);

  final BleCommandChannel _channel;

  Future<DeviceInfo> deviceInfo() async => DeviceInfo.fromJson(await _channel.request(BleOp.deviceInfo));

  Future<void> setSecret(String key, String value) =>
      _channel.request(BleOp.secretSet, {'key': key, 'value': value});

  Future<void> eraseSecret(String key) => _channel.request(BleOp.secretErase, {'key': key});

  /// 휴대폰 등록. 가득 차면 [BleCommandException] `PHONES_FULL` (data['phones'] 목록) →
  /// 사용자 확인 뒤 [evict]를 붙여 다시 부른다
  Future<PairResult> pair({required String phoneId, required String phoneName, String? evict}) async {
    final r = await _channel.request(BleOp.apiPair, {
      'phone_id': phoneId,
      'phone_name': phoneName,
      'evict': ?evict,
    });
    return PairResult(token: r['token'] as String, phoneId: r['phone_id'] as String? ?? phoneId);
  }

  /// 저장 후 기기가 바로 토큰 갱신을 시도해 결과를 준다
  Future<void> linkSpotify({required String clientId, required String refreshToken}) =>
      _channel.request(BleOp.spotifyLink, {'client_id': clientId, 'refresh_token': refreshToken});

  Future<void> logoutSpotify() => _channel.request(BleOp.spotifyLogout);

  Future<Map<String, dynamic>> getSettings(String group) async =>
      (await _channel.request(BleOp.settingsGet, {'group': group}))['value'] as Map<String, dynamic>? ?? const {};

  Future<void> setSettings(String group, Map<String, Object?> value) =>
      _channel.request(BleOp.settingsSet, {'group': group, 'value': value});

  /// 쓴 값 적용, BLE 창 닫기, 재부팅 없음
  Future<void> close() => _channel.request(BleOp.provClose);
}
