/// 시스템 화면의 값 → 문구 (테스트하기 쉽게 화면 밖에 둔다)
library;

import '../../../l10n/app_localizations.dart';
import '../domain/entities/system_status.dart';

/// "3시간 12분", 하루가 넘으면 "2일 3시간"
String formatUptime(AppLocalizations l, Duration d) {
  if (d.inDays > 0) return l.sysUptimeDays(d.inDays, d.inHours % 24);
  if (d.inHours > 0) return l.sysUptimeHours(d.inHours, d.inMinutes % 60);
  return l.sysUptimeMinutes(d.inMinutes);
}

/// "좋음 (-51 dBm)", 연결 안 됐으면 "연결 안 됨"
String formatWifi(AppLocalizations l, int? rssi) {
  if (rssi == null) return l.sysWifiNone;
  return switch (WifiBand.fromRssi(rssi)) {
    WifiBand.good => l.sysWifiGood(rssi),
    WifiBand.fair => l.sysWifiFair(rssi),
    WifiBand.weak => l.sysWifiWeak(rssi),
  };
}

String formatResetReason(AppLocalizations l, int code) => switch (ResetCause.fromCode(code)) {
      ResetCause.powerOn => l.sysResetPowerOn,
      ResetCause.software => l.sysResetSoftware,
      ResetCause.crash => l.sysResetCrash,
      ResetCause.watchdog => l.sysResetWatchdog,
      ResetCause.brownout => l.sysResetBrownout,
      ResetCause.other => l.sysResetOther(code),
    };

String formatKb(AppLocalizations l, int? bytes) => bytes == null ? '–' : l.sysKb(bytes ~/ 1024);
