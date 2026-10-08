/// BLE GATT 규격 — 펌웨어 `app_config.h`와 설계서 2.13절과 1:1로 같아야 하는 유일한 파일.
/// 값을 바꾸면 펌웨어도 같이 바꾼다.
library;

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BleProtocol {
  BleProtocol._();

  /// 광고 이름 접두어. 뒤 6자리 = Wi-Fi STA MAC 뒤 6자리
  static const namePrefix = 'JARVIS_';

  /// `device.info`의 `protocol_version`. 펌웨어가 더 크면 앱 업데이트 안내
  static const protocolVersion = 1;

  // 서비스·특성 (16비트)
  static final svcWifi = Guid('ff50');
  static final chrSsid = Guid('ff51');
  static final chrPassword = Guid('ff52');
  static final chrStatus = Guid('ff53');
  static final chrScan = Guid('ff54');

  static final svcDevice = Guid('ff60');
  static final chrMac = Guid('ff61');
  static final chrProduct = Guid('ff62');
  static final chrFirmware = Guid('ff63');

  static final svcConfig = Guid('ff80');
  static final chrCmdRx = Guid('ff81');
  static final chrCmdTx = Guid('ff82');
  static final chrProvDone = Guid('ff88');

  static const productCode = 'AISPK-S3-V1';

  // 길이 제한 (UTF-8 바이트)
  static const ssidMaxBytes = 32;
  static const passwordMaxBytes = 64;

  // 설정 명령 프로토콜 (0xFF81/0xFF82)
  static const flagLast = 0x01;
  static const flagFirst = 0x02;
  static const frameHeaderBytes = 2;

  /// ATT 헤더 3B + 조각 헤더 2B. 조각 payload = MTU − 5
  static const mtuOverhead = 5;
  static const androidMtuRequest = 247;
  static const cmdMaxBytes = 4096;
  static const maxFrames = 256;
  static const cmdTimeout = Duration(seconds: 10);

  /// 0xFF88에 쓰는 값
  static const provDoneValue = 0x01;

  /// Key2 3초로 연 BLE 창: 연결 없으면 5분, 연결 중 최대 15분
  static const windowIdle = Duration(minutes: 5);
  static const windowMax = Duration(minutes: 15);

  // 0xFF54 스캔 알림
  static const scanEntryBytes = 34;
  static const scanFailed = 0xFF;
  static const scanMaxEntries = 20;
}

/// 0xFF53 Wi-Fi 상태. 2–5는 종료 상태이고 비밀번호를 쓸 때마다 한 번만 온다.
enum WifiProvStatus {
  idle(0),
  connecting(1),
  success(2),
  ssidNotFound(3),
  authFailed(4),
  timeout(5);

  const WifiProvStatus(this.code);
  final int code;

  bool get isFinal => code >= 2;

  static WifiProvStatus? fromCode(int code) {
    for (final s in values) {
      if (s.code == code) return s;
    }
    return null;
  }
}

/// 0xFF54 보안 값 (IDF 숫자가 아니라 설계서 표)
enum WifiSecurity {
  open(0),
  wep(1),
  wpa(2),
  wpa2(3),
  wpaWpa2(4),
  wpa3(5),
  wpa2Wpa3(6),
  enterprise(7),
  other(255);

  const WifiSecurity(this.code);
  final int code;

  bool get needsPassword => this != open;

  static WifiSecurity fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => other);
}

/// 0xFF81 명령 이름
class BleOp {
  BleOp._();
  static const deviceInfo = 'device.info';
  static const secretSet = 'secret.set';
  static const secretErase = 'secret.erase';
  static const settingsGet = 'settings.get';
  static const settingsSet = 'settings.set';
  static const apiPair = 'api.pair';
  static const spotifyLink = 'spotify.link';
  static const spotifyLogout = 'spotify.logout';
  static const provClose = 'prov.close';
}

/// `secret.set`·`secret.erase` 키
class SecretKey {
  SecretKey._();
  static const openai = 'openai_key';
  static const gemini = 'gemini_key';
  static const telegramBot = 'tg_bot_token';
}

/// 0xFF82 응답 오류 코드
class BleCmdError {
  BleCmdError._();
  static const invalidArg = 'INVALID_ARG';
  static const unknownOp = 'UNKNOWN_OP';
  static const tooLarge = 'TOO_LARGE';
  static const frameTimeout = 'FRAME_TIMEOUT';
  static const busy = 'BUSY';
  static const storageFail = 'STORAGE_FAIL';
  static const notAllowed = 'NOT_ALLOWED';
  static const phonesFull = 'PHONES_FULL';
  static const unsupportedVersion = 'UNSUPPORTED_VERSION';

  /// `settings.set assistant`에서 키가 없는 AI를 고름 (LAN은 409 같은 이름)
  static const noApiKey = 'NO_API_KEY';
}
