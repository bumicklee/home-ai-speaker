/// AI 회사 × 대화 방식 (설계서 docs/design-voice-saver-mode.md 11.1·11.4절). 펌웨어 `assistant` 설정·`/status.ai`와 같은 이름.
library;

import 'package:equatable/equatable.dart';

import '../constants/ble_protocol.dart';

/// 비서 설정 `ai_provider`
enum AiProvider {
  openai('openai', SecretKey.openai),
  gemini('gemini', SecretKey.gemini);

  const AiProvider(this.id, this.secretKey);

  final String id;

  /// 스피커 보안 저장소의 키 이름 (`secret.set`, `/status.secrets`)
  final String secretKey;

  /// 화면 이름 (번역하지 않는 회사 이름)
  String get displayName => switch (this) {
        openai => 'OpenAI',
        gemini => 'Gemini',
      };

  /// 비서 설정에서 이 회사의 방식을 담는 키 (`mode_openai`, `mode_gemini`)
  String get modeSettingKey => 'mode_$id';

  AiProvider get other => this == openai ? gemini : openai;

  static AiProvider? tryParse(Object? id) {
    for (final p in values) {
      if (p.id == id) return p;
    }
    return null;
  }
}

/// `mode_openai`·`mode_gemini`: 실시간 / 절약
enum AiMode {
  live('live'),
  saver('saver');

  const AiMode(this.id);

  final String id;

  static AiMode? tryParse(Object? id) {
    for (final m in values) {
      if (m.id == id) return m;
    }
    return null;
  }
}

/// `"openai.saver"` 같은 조합 이름 (`/status.ai.available`)
String aiCombo(AiProvider p, AiMode m) => '${p.id}.${m.id}';

/// `GET /status`의 `ai` — 지금 쓰는 AI와 이 펌웨어가 할 수 있는 조합
class AiStatus extends Equatable {
  const AiStatus({required this.provider, required this.mode, required this.available, this.reported = true});

  /// 옛 펌웨어(`ai` 없음): OpenAI 실시간만
  static const legacy = AiStatus(
    provider: AiProvider.openai,
    mode: AiMode.live,
    available: {'openai.live'},
    reported: false,
  );

  /// 빠지거나 모르는 값은 옛 펌웨어처럼 본다
  factory AiStatus.fromJson(Object? json) {
    if (json is! Map) return legacy;
    final list = json['available'];
    final available = <String>{
      if (list is List)
        for (final a in list)
          if (a is String) a,
    };
    return AiStatus(
      provider: AiProvider.tryParse(json['provider']) ?? AiProvider.openai,
      mode: AiMode.tryParse(json['mode']) ?? AiMode.live,
      available: list is List ? available : legacy.available,
    );
  }

  final AiProvider provider;
  final AiMode mode;

  /// 이 펌웨어가 지금 할 수 있는 조합 (`openai.live` …)
  final Set<String> available;

  /// 펌웨어가 `ai`를 알려 줬는지 (옛 펌웨어면 false)
  final bool reported;

  bool isAvailable(AiProvider p, AiMode m) => available.contains(aiCombo(p, m));

  /// 이 회사로 할 수 있는 방식이 하나라도 있는지
  bool providerAvailable(AiProvider p) => AiMode.values.any((m) => isAvailable(p, m));

  @override
  List<Object?> get props => [provider, mode, available, reported];
}
