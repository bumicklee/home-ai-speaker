import '../../../../core/ai/ai_choice.dart';
import '../../domain/entities/assistant_settings.dart';

/// 펌웨어 `assistant.c` 설정 JSON ↔ AssistantSettings. 빠진 키는 펌웨어 기본값(DEFAULTS)으로 채운다.
/// AI 키 3개가 없으면 옛 펌웨어: OpenAI 실시간 (설계 docs/design-voice-saver-mode.md 11.1 "옮겨 온 스피커")
class AssistantSettingsModel {
  AssistantSettingsModel._();

  static const keyName = 'name';
  static const keyPurpose = 'purpose';
  static const keyLanguage = 'language';
  static const keyVoice = 'voice';
  static const keyLocation = 'location';
  static const keyUnits = 'units';
  static const keyWebSearch = 'web_search';
  static const keyDeviceControls = 'device_controls';
  static const keyAutoMemory = 'auto_memory';
  static const keyTimezone = 'timezone';
  static const keyAiProvider = 'ai_provider';
  static const keyModeOpenai = 'mode_openai';
  static const keyModeGemini = 'mode_gemini';

  static AssistantSettings fromJson(Map<String, dynamic> j) => AssistantSettings(
        name: j[keyName] as String? ?? 'Jarvis',
        purpose: j[keyPurpose] as String? ?? '',
        language: j[keyLanguage] as String? ?? 'ko',
        voice: j[keyVoice] as String? ?? 'marin',
        location: j[keyLocation] as String? ?? '',
        units: j[keyUnits] as String? ?? 'metric',
        webSearch: j[keyWebSearch] as bool? ?? true,
        deviceControls: j[keyDeviceControls] as bool? ?? true,
        autoMemory: j[keyAutoMemory] as bool? ?? false,
        timezone: j[keyTimezone] as String? ?? 'Asia/Seoul',
        aiProvider: AiProvider.tryParse(j[keyAiProvider]) ?? AiProvider.openai,
        modeOpenai: AiMode.tryParse(j[keyModeOpenai]) ?? AiMode.live,
        modeGemini: AiMode.tryParse(j[keyModeGemini]) ?? AiMode.saver,
      );

  static Map<String, dynamic> toJson(AssistantSettings s) => {
        keyName: s.name,
        keyPurpose: s.purpose,
        keyLanguage: s.language,
        keyVoice: s.voice,
        keyLocation: s.location,
        keyUnits: s.units,
        keyWebSearch: s.webSearch,
        keyDeviceControls: s.deviceControls,
        keyAutoMemory: s.autoMemory,
        keyTimezone: s.timezone,
        keyAiProvider: s.aiProvider.id,
        keyModeOpenai: s.modeOpenai.id,
        keyModeGemini: s.modeGemini.id,
      };

  /// [edited]에서 [original]과 다른 키만 (PUT 본문)
  static Map<String, dynamic> diff(AssistantSettings original, AssistantSettings edited) {
    final before = toJson(original);
    return {
      for (final e in toJson(edited).entries)
        if (before[e.key] != e.value) e.key: e.value,
    };
  }
}
