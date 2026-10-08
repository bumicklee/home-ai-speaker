import '../../../../core/ai/ai_choice.dart';
import '../../domain/entities/device_status.dart';

/// 펌웨어 `device_info.c`의 `/status` JSON → DeviceStatus
class DeviceStatusModel {
  static DeviceStatus fromJson(Map<String, dynamic> j) {
    final tg = j['telegram'] as Map<String, dynamic>? ?? const {};
    return DeviceStatus(
      firmware: j['fw'] as String? ?? '',
      uptime: Duration(seconds: (j['uptime_s'] as num? ?? 0).toInt()),
      wifiRssi: (j['wifi_rssi'] as num?)?.toInt(),
      voiceState: j['voice_state'] as String? ?? 'idle',
      media: mediaFromJson(j['media'] as Map<String, dynamic>? ?? const {}),
      volume: (j['volume'] as num? ?? 0).toInt(),
      secrets: {
        for (final e in (j['secrets'] as Map<String, dynamic>? ?? const {}).entries) e.key: e.value == true,
      },
      telegramState: tg['state'] as String? ?? 'off',
      telegramUnread: (tg['unread'] as num? ?? 0).toInt(),
      spotifyState: j['spotify_state'] as String? ?? 'off',
      ai: AiStatus.fromJson(j['ai']),
    );
  }

  /// `/media/now`, WS `media_state` (event 키는 무시)
  static MediaNow mediaFromJson(Map<String, dynamic> j) => MediaNow(
        source: j['source'] as String? ?? 'none',
        state: j['state'] as String? ?? 'stopped',
        title: j['title'] as String? ?? '',
        artist: j['artist'] as String? ?? '',
        station: j['station'] as String? ?? '',
      );
}
