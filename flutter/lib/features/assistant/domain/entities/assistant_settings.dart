import 'dart:convert';

import 'package:equatable/equatable.dart';

import '../../../../core/ai/ai_choice.dart';

/// 펌웨어 `assistant.c`의 검증과 같은 값 (settings_valid, TIMEZONES)
class AssistantLimits {
  AssistantLimits._();

  /// UTF-8 바이트 수 (글자 수가 아님. 한글 한 글자 = 3바이트)
  static const nameBytes = 32;
  static const purposeBytes = 600;
  static const locationBytes = 96;

  /// 기억 최대 개수 (MEMORY_LIMIT)
  static const memories = 12;

  /// 말하는 언어 (assistant.c: ko | en)
  static const languages = ['ko', 'en'];

  /// 단위 (metric = °C·km, imperial = °F·mi)
  static const units = ['metric', 'imperial'];

  /// 칩에 있는 시간대만 (assistant.c `TIMEZONES`). 이 밖의 이름은 INVALID_ARG
  static const timezones = [
    'Asia/Seoul',
    'Asia/Tokyo',
    'Asia/Shanghai',
    'Asia/Singapore',
    'UTC',
    'Europe/London',
    'Europe/Berlin',
    'America/New_York',
    'America/Chicago',
    'America/Denver',
    'America/Los_Angeles',
    'Australia/Sydney',
  ];
}

int utf8Bytes(String s) => utf8.encode(s).length;

/// 저장 전 검사에서 걸린 항목
enum AssistantFieldError { nameEmpty, nameTooLong, purposeTooLong, locationTooLong }

/// `/api/v1/settings/assistant` (키 13개: 옛 10개 + ai_provider·mode_openai·mode_gemini)
class AssistantSettings extends Equatable {
  const AssistantSettings({
    required this.name,
    required this.purpose,
    required this.language,
    required this.voice,
    required this.location,
    required this.units,
    required this.webSearch,
    required this.deviceControls,
    required this.autoMemory,
    required this.timezone,
    this.aiProvider = AiProvider.openai,
    this.modeOpenai = AiMode.live,
    this.modeGemini = AiMode.saver,
  });

  final String name;
  final String purpose;
  final String language;

  /// 다음 대화부터 바뀐다
  final String voice;

  /// 동네 이름만 (좌표는 보내지 않는다. 설계서 3.0절)
  final String location;
  final String units;
  final bool webSearch;

  /// 끄면 볼륨·라디오·기억 등 기기 툴을 비서에게 주지 않는다
  final bool deviceControls;
  final bool autoMemory;
  final String timezone;

  /// 대화에 쓰는 AI (키가 없는 AI로 바꾸면 기기가 409 NO_API_KEY)
  final AiProvider aiProvider;

  /// 회사마다 마지막으로 고른 방식 (회사를 바꾸면 그 회사의 방식이 된다)
  final AiMode modeOpenai;
  final AiMode modeGemini;

  AiMode modeOf(AiProvider p) => p == AiProvider.openai ? modeOpenai : modeGemini;

  /// 지금 쓰는 방식
  AiMode get aiMode => modeOf(aiProvider);

  /// 기기가 거부할 값을 미리 찾는다. 없으면 빈 목록
  List<AssistantFieldError> validate() => [
        if (name.trim().isEmpty) AssistantFieldError.nameEmpty,
        if (utf8Bytes(name) > AssistantLimits.nameBytes) AssistantFieldError.nameTooLong,
        if (utf8Bytes(purpose) > AssistantLimits.purposeBytes) AssistantFieldError.purposeTooLong,
        if (utf8Bytes(location) > AssistantLimits.locationBytes) AssistantFieldError.locationTooLong,
      ];

  AssistantSettings copyWith({
    String? name,
    String? purpose,
    String? language,
    String? voice,
    String? location,
    String? units,
    bool? webSearch,
    bool? deviceControls,
    bool? autoMemory,
    String? timezone,
    AiProvider? aiProvider,
    AiMode? modeOpenai,
    AiMode? modeGemini,
  }) =>
      AssistantSettings(
        name: name ?? this.name,
        purpose: purpose ?? this.purpose,
        language: language ?? this.language,
        voice: voice ?? this.voice,
        location: location ?? this.location,
        units: units ?? this.units,
        webSearch: webSearch ?? this.webSearch,
        deviceControls: deviceControls ?? this.deviceControls,
        autoMemory: autoMemory ?? this.autoMemory,
        timezone: timezone ?? this.timezone,
        aiProvider: aiProvider ?? this.aiProvider,
        modeOpenai: modeOpenai ?? this.modeOpenai,
        modeGemini: modeGemini ?? this.modeGemini,
      );

  @override
  List<Object?> get props => [
        name,
        purpose,
        language,
        voice,
        location,
        units,
        webSearch,
        deviceControls,
        autoMemory,
        timezone,
        aiProvider,
        modeOpenai,
        modeGemini,
      ];
}

/// 비서 기억 하나 (키: 소문자·숫자·_ 1–32자, 값: 1–160바이트)
class AssistantMemory extends Equatable {
  const AssistantMemory({required this.key, required this.value});

  final String key;
  final String value;

  @override
  List<Object?> get props => [key, value];
}
