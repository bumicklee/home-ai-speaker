/// AI·대화 방식 문구 (첫 설정, 비서 > AI · 대화 방식, 홈, 영어 선생님이 같이 쓴다)
library;

import '../../l10n/app_localizations.dart';
import 'ai_choice.dart';

extension AiTexts on AppLocalizations {
  String aiModeName(AiMode m) => m == AiMode.saver ? aiModeSaver : aiModeLive;

  /// "OpenAI · 절약 모드"
  String aiChoiceLabel(AiProvider p, AiMode m) => aiLabel(p.displayName, aiModeName(m));

  /// 첫 설정 카드 한 줄: "월 약 1.5만 원 · 대답 1~2초 늦음"
  String aiShortSub(AiProvider p, AiMode m) {
    final price = switch ((p, m)) {
      (AiProvider.openai, AiMode.saver) => aiPriceShortOpenAiSaver,
      (AiProvider.openai, AiMode.live) => aiPriceShortOpenAiLive,
      (AiProvider.gemini, AiMode.saver) => aiPriceShortGeminiSaver,
      (AiProvider.gemini, AiMode.live) => aiPriceShortGeminiLive,
    };
    return '$price · ${m == AiMode.saver ? aiShortSaverNote : aiShortLiveNote}';
  }

  /// 한 달 예상 (하루 1시간)
  String aiPrice(AiProvider p, AiMode m) => switch ((p, m)) {
        (AiProvider.openai, AiMode.saver) => aiPriceOpenAiSaver,
        (AiProvider.openai, AiMode.live) => aiPriceOpenAiLive,
        (AiProvider.gemini, AiMode.saver) => aiPriceGeminiSaver,
        (AiProvider.gemini, AiMode.live) => aiPriceGeminiLive,
      };

  String aiDescription(AiProvider p, AiMode m) => switch ((p, m)) {
        (AiProvider.openai, AiMode.saver) => aiDescOpenAiSaver,
        (AiProvider.openai, AiMode.live) => aiDescOpenAiLive,
        (AiProvider.gemini, AiMode.saver) => aiDescGeminiSaver,
        (AiProvider.gemini, AiMode.live) => aiDescGeminiLive,
      };

  List<String> aiTags(AiProvider p, AiMode m) => m == AiMode.live
      ? [aiTagInstant, aiTagBargeIn]
      : [p == AiProvider.openai ? aiTagOneEighth : aiTagCheapest, aiTagNoBargeIn];
}
