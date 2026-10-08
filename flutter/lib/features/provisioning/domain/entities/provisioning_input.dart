import 'package:equatable/equatable.dart';

import '../../../../core/ai/ai_choice.dart';

/// BLE 연결 **전에** 받는 값 (설계서 3.4: 키를 복사하러 앱을 벗어나는 동안 BLE 창 시간이 흐르지 않게).
/// 메모리에만 두고 전송 뒤 버린다.
/// ① AI → 그 AI의 키(필수, 또는 "이미 키가 저장된 스피커") → ② 대화 방식 (docs/design-voice-saver-mode.md 11.3절)
class ProvisioningInput extends Equatable {
  const ProvisioningInput({
    this.provider = AiProvider.openai,
    this.mode = AiMode.saver,
    this.openAiKey = '',
    this.geminiKey = '',
    this.existingKey = false,
    this.telegramToken = '',
    required this.phoneName,
  });

  final AiProvider provider;
  final AiMode mode;

  /// 비우면 스피커에 이미 있는 키를 쓴다 (고른 AI라면 [existingKey]를 켜야 한다)
  final String openAiKey;
  final String geminiKey;

  /// "이미 이 AI 키가 저장된 스피커예요" — 연결 뒤 `device.info.secrets`로 확인한다
  final bool existingKey;
  final String telegramToken;
  final String phoneName;

  String keyFor(AiProvider p) => p == AiProvider.openai ? openAiKey : geminiKey;

  /// 고른 AI의 키를 넣었는지
  bool get hasChosenKey => keyFor(provider).isNotEmpty;

  /// "스피커 찾기"를 누를 수 있는지: 고른 AI의 키를 넣었거나, 스피커에 이미 있다고 했거나
  bool get canSearch => hasChosenKey || existingKey;

  ProvisioningInput copyWith({
    AiProvider? provider,
    AiMode? mode,
    String? openAiKey,
    String? geminiKey,
    bool? existingKey,
  }) =>
      ProvisioningInput(
        provider: provider ?? this.provider,
        mode: mode ?? this.mode,
        openAiKey: openAiKey ?? this.openAiKey,
        geminiKey: geminiKey ?? this.geminiKey,
        existingKey: existingKey ?? this.existingKey,
        telegramToken: telegramToken,
        phoneName: phoneName,
      );

  static final openAiKeyPattern = RegExp(r'^sk-[A-Za-z0-9_\-]{20,}$');

  /// Google AI Studio 키 (느슨하게: AIza + 30자 이상)
  static final geminiKeyPattern = RegExp(r'^AIza[0-9A-Za-z_\-]{30,}$');
  static final telegramTokenPattern = RegExp(r'^\d{5,}:[A-Za-z0-9_\-]{30,}$');

  static RegExp keyPattern(AiProvider p) => p == AiProvider.openai ? openAiKeyPattern : geminiKeyPattern;

  @override
  List<Object?> get props => [provider, mode, openAiKey, geminiKey, existingKey, telegramToken, phoneName];

  /// 로그·toString에 비밀값이 나오지 않게
  @override
  String toString() => 'ProvisioningInput(phoneName: $phoneName, ai: ${aiCombo(provider, mode)}, '
      'openai: ${openAiKey.isNotEmpty}, gemini: ${geminiKey.isNotEmpty}, existing: $existingKey, '
      'tg: ${telegramToken.isNotEmpty})';
}
