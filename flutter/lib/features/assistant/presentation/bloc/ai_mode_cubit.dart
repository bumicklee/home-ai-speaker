import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/ai/ai_choice.dart';
import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/storage/key_vault.dart';
import '../../../system/domain/repositories/system_repository.dart';
import '../../data/models/assistant_settings_model.dart';
import '../../domain/entities/assistant_settings.dart';
import '../../domain/repositories/assistant_repository.dart';

/// AI·방식 카드를 누를 수 없는 이유
enum AiLock {
  none,

  /// 스피커에 이 회사 키가 없음 ("키 없음 · 쓸 수 없어요")
  noKey,

  /// 이 펌웨어가 아직 못 함 (`/status.ai.available`에 없음, "준비 중")
  notReady,
}

class AiModeState extends Equatable {
  const AiModeState({
    this.settings,
    this.secrets,
    this.ai = AiStatus.legacy,
    this.loading = false,
    this.saving = false,
    this.failure,
  });

  /// 스피커의 비서 설정 (ai_provider, mode_openai, mode_gemini)
  final AssistantSettings? settings;

  /// `/status.secrets`. 상태를 못 읽었으면 null (키로는 잠그지 않는다. 스피커가 한 번 더 막는다)
  final Map<String, bool>? secrets;
  final AiStatus ai;
  final bool loading;

  /// 바꾸는 중 (카드를 누를 수 없다)
  final bool saving;

  /// 불러오기 실패 (화면 전체)
  final Failure? failure;

  AiProvider get provider => settings?.aiProvider ?? AiProvider.openai;
  AiMode get mode => settings?.aiMode ?? AiMode.live;

  /// 스피커에 키가 있는지 (모르면 null)
  bool? hasKey(AiProvider p) => secrets == null ? null : (secrets![p.secretKey] ?? false);

  AiLock providerLock(AiProvider p) {
    if (hasKey(p) == false) return AiLock.noKey;
    if (!ai.providerAvailable(p)) return AiLock.notReady;
    return AiLock.none;
  }

  AiLock modeLock(AiProvider p, AiMode m) => ai.isAvailable(p, m) ? AiLock.none : AiLock.notReady;

  AiModeState copyWith({
    AssistantSettings? settings,
    Map<String, bool>? Function()? secrets,
    AiStatus? ai,
    bool? loading,
    bool? saving,
    Failure? Function()? failure,
  }) =>
      AiModeState(
        settings: settings ?? this.settings,
        secrets: secrets != null ? secrets() : this.secrets,
        ai: ai ?? this.ai,
        loading: loading ?? this.loading,
        saving: saving ?? this.saving,
        failure: failure != null ? failure() : this.failure,
      );

  @override
  List<Object?> get props => [settings, secrets, ai, loading, saving, failure];
}

/// 비서 > AI · 대화 방식 (설계 docs/design-voice-saver-mode.md 9·11.8절).
/// 바꾸면 바로 LAN `PUT /settings/assistant` (부분). 키는 BLE로만 보낸다 (시스템 > 키와 같은 길)
class AiModeCubit extends Cubit<AiModeState> {
  AiModeCubit(this._assistant, this._system, this._vault) : super(const AiModeState());

  final AssistantRepository _assistant;
  final SystemRepository _system;
  final KeyVault _vault;

  /// 스피커가 키가 없다고 거절함 (LAN 409 / BLE `NO_API_KEY`)
  static bool isNoKey(Failure f) => f.code == BleCmdError.noApiKey;

  Future<void> load() async {
    emit(state.copyWith(loading: true, failure: () => null));
    final settings = _assistant.settings();
    final status = _system.status();
    final s = await settings;
    final st = await status;
    if (isClosed) return;
    s.fold(
      (f) => emit(state.copyWith(loading: false, failure: () => f)),
      (v) => st.fold(
        (_) => emit(AiModeState(settings: v)),
        (x) => emit(AiModeState(settings: v, secrets: x.secrets, ai: x.ai)),
      ),
    );
  }

  /// ① AI 바꾸기. 그 회사의 마지막 방식이 이 스피커에서 안 되면 되는 방식으로 함께 바꾼다
  Future<Failure?> chooseProvider(AiProvider p) async {
    final s = state.settings;
    if (s == null || state.saving || p == s.aiProvider || state.providerLock(p) != AiLock.none) return null;
    var m = s.modeOf(p);
    if (!state.ai.isAvailable(p, m)) {
      m = AiMode.values.firstWhere((x) => state.ai.isAvailable(p, x), orElse: () => m);
    }
    final changes = <String, dynamic>{
      AssistantSettingsModel.keyAiProvider: p.id,
      if (m != s.modeOf(p)) p.modeSettingKey: m.id,
    };
    return _put(changes, provider: p);
  }

  /// ② 지금 AI의 방식 바꾸기
  Future<Failure?> chooseMode(AiMode m) async {
    final s = state.settings;
    final p = state.provider;
    if (s == null || state.saving || m == s.modeOf(p) || state.modeLock(p, m) != AiLock.none) return null;
    return _put({p.modeSettingKey: m.id}, provider: p);
  }

  Future<Failure?> _put(Map<String, dynamic> changes, {required AiProvider provider}) async {
    emit(state.copyWith(saving: true));
    final r = await _assistant.update(changes);
    if (isClosed) return r.fold((f) => f, (_) => null);
    return r.fold((f) {
      // 스피커에서 키가 지워졌다: 카드를 잠근다
      final secrets =
          isNoKey(f) && state.secrets != null ? {...state.secrets!, provider.secretKey: false} : state.secrets;
      emit(state.copyWith(saving: false, secrets: () => secrets));
      return f;
    }, (saved) {
      emit(state.copyWith(saving: false, settings: saved));
      return null;
    });
  }

  /// 휴대폰에 기억한 키 ("키 넣기" 칸을 미리 채운다)
  Future<String> rememberedKey(AiProvider p) async =>
      (p == AiProvider.openai ? await _vault.openAiKey() : await _vault.geminiKey()) ?? '';

  /// "○○ 키 넣기": BLE `secret.set` (사용자가 Key2를 3초 눌러 BLE 창을 연 뒤). 성공하면 휴대폰에도 기억
  Future<Failure?> sendKey(AiProvider p, String value) async {
    final r = await _system.sendKeyOverBle(p.secretKey, value);
    if (isClosed) return r.fold((f) => f, (_) => null);
    final failure = r.fold((f) => f, (_) => null);
    if (failure != null) return failure;
    await (p == AiProvider.openai ? _vault.saveOpenAiKey(value) : _vault.saveGeminiKey(value));
    if (isClosed) return null;
    emit(state.copyWith(secrets: () => {...?state.secrets, p.secretKey: true}));
    return null;
  }
}
