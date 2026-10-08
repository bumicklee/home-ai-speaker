import 'package:equatable/equatable.dart';

import '../../../../core/ai/ai_choice.dart';

/// Wi-Fi 신호 구간 (홈 화면과 같은 기준)
enum WifiBand {
  good,
  fair,
  weak;

  static WifiBand fromRssi(int rssi) => rssi >= -60
      ? good
      : rssi >= -72
          ? fair
          : weak;
}

/// 스피커가 마지막으로 켜진 이유 (`esp_reset_reason_t`를 화면용으로 묶음)
enum ResetCause {
  powerOn, // 1 전원
  software, // 3 앱·음성으로 다시 시작, OTA
  crash, // 4 오류 (panic)
  watchdog, // 5–7 응답 없음
  brownout, // 9 전원 부족
  other;

  static ResetCause fromCode(int code) => switch (code) {
        1 => powerOn,
        3 => software,
        4 => crash,
        5 || 6 || 7 => watchdog,
        9 => brownout,
        _ => other,
      };
}

/// `/api/v1/status` 중 시스템 화면에 쓰는 것 (펌웨어 `device_info.c` device_status)
class SystemStatus extends Equatable {
  const SystemStatus({
    required this.firmware,
    this.buildDate = '',
    required this.uptime,
    this.wifiRssi,
    this.resetReason = 0,
    this.heapInternalFree,
    this.heapInternalMin,
    this.psramFree,
    this.voiceState = 'idle',
    this.secrets = const {},
    this.failures = const [],
    this.ai = AiStatus.legacy,
  });

  final String firmware;

  /// 빌드 시각 ("Oct  3 2026 10:12:00", 컴파일러 형식 그대로)
  final String buildDate;
  final Duration uptime;

  /// Wi-Fi에 안 붙어 있으면 null
  final int? wifiRssi;

  /// `esp_reset_reason()` 숫자
  final int resetReason;

  /// 내부 RAM (바이트)
  final int? heapInternalFree;
  final int? heapInternalMin;
  final int? psramFree;
  final String voiceState;

  /// 저장 여부만 (openai_key, gemini_key, tg_bot_token, spotify, stock_key)
  final Map<String, bool> secrets;

  /// 최근 실패 (펌웨어가 아직 비워 둔다)
  final List<String> failures;

  /// 지금 쓰는 AI와 이 펌웨어가 할 수 있는 조합 (옛 펌웨어면 [AiStatus.legacy])
  final AiStatus ai;

  WifiBand? get wifiBand => wifiRssi == null ? null : WifiBand.fromRssi(wifiRssi!);
  ResetCause get resetCause => ResetCause.fromCode(resetReason);
  bool hasSecret(String key) => secrets[key] ?? false;

  SystemStatus withSecret(String key, bool saved) => SystemStatus(
        firmware: firmware,
        buildDate: buildDate,
        uptime: uptime,
        wifiRssi: wifiRssi,
        resetReason: resetReason,
        heapInternalFree: heapInternalFree,
        heapInternalMin: heapInternalMin,
        psramFree: psramFree,
        voiceState: voiceState,
        secrets: {...secrets, key: saved},
        failures: failures,
        ai: ai,
      );

  @override
  List<Object?> get props => [
        firmware,
        buildDate,
        uptime,
        wifiRssi,
        resetReason,
        heapInternalFree,
        heapInternalMin,
        psramFree,
        voiceState,
        secrets,
        failures,
        ai,
      ];
}

/// 스피커 한 대에 등록할 수 있는 휴대폰 수 (펌웨어 `APP_API_MAX_PHONES`)
const maxPhones = 5;

/// `GET /system/phones`의 한 줄
class RegisteredPhone extends Equatable {
  const RegisteredPhone({required this.phoneId, required this.name, this.created, this.self = false});

  final String phoneId;
  final String name;

  /// 등록 시각. 스피커 시계가 맞기 전에 등록했으면 null
  final DateTime? created;

  /// 지금 요청한 휴대폰 (이 휴대폰)
  final bool self;

  @override
  List<Object?> get props => [phoneId, name, created, self];
}
