import 'dart:math';

import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/storage/key_vault.dart';
import '../../domain/repositories/system_repository.dart';

class KeysState extends Equatable {
  const KeysState({
    this.loaded = false,
    this.onPhone = const {},
    this.onSpeaker,
    this.speakerFailure,
    this.sending,
    this.busy = false,
  });

  final bool loaded;

  /// 휴대폰(KeyVault)에 기억해 둔 값. 키 → 값 (없으면 빠짐)
  final Map<String, String> onPhone;

  /// 스피커 저장 여부 (`/status` secrets). 연결이 안 되면 null
  final Map<String, bool>? onSpeaker;
  final Failure? speakerFailure;

  /// BLE로 보내는 중인 키
  final String? sending;

  /// LAN 삭제 중
  final bool busy;

  KeysState copyWith({
    bool? loaded,
    Map<String, String>? onPhone,
    Map<String, bool>? Function()? onSpeaker,
    Failure? Function()? speakerFailure,
    String? Function()? sending,
    bool? busy,
  }) =>
      KeysState(
        loaded: loaded ?? this.loaded,
        onPhone: onPhone ?? this.onPhone,
        onSpeaker: onSpeaker != null ? onSpeaker() : this.onSpeaker,
        speakerFailure: speakerFailure != null ? speakerFailure() : this.speakerFailure,
        sending: sending != null ? sending() : this.sending,
        busy: busy ?? this.busy,
      );

  @override
  List<Object?> get props => [loaded, onPhone, onSpeaker, speakerFailure, sending, busy];
}

/// 키 관리: 이 휴대폰에 기억한 키(KeyVault)와 스피커에 저장된 키.
/// 스피커로 보낼 때는 BLE만 쓴다 (D6). 지우기는 LAN `DELETE /keys/{key}`
class KeysCubit extends Cubit<KeysState> {
  KeysCubit(this._repo, this._vault) : super(const KeysState());

  final SystemRepository _repo;
  final KeyVault _vault;

  /// 화면에서 다루는 키 (스피커 `secret.set` 이름)
  static const keys = [SecretKey.openai, SecretKey.gemini, SecretKey.telegramBot];

  /// 뒤 4자리만 보인다
  static String mask(String value) => '••••${value.substring(max(0, value.length - 4))}';

  Future<void> load() async {
    final phone = await _readVault();
    if (isClosed) return;
    emit(state.copyWith(loaded: true, onPhone: phone));
    final r = await _repo.status();
    if (isClosed) return;
    r.fold(
      (f) => emit(state.copyWith(onSpeaker: () => null, speakerFailure: () => f)),
      (s) => emit(state.copyWith(
        onSpeaker: () => {for (final k in keys) k: s.hasSecret(k)},
        speakerFailure: () => null,
      )),
    );
  }

  /// 이 휴대폰에서만 지운다 (스피커의 키는 그대로)
  Future<void> forgetOnPhone(String key) async {
    await _save(key, null);
    if (isClosed) return;
    emit(state.copyWith(onPhone: {...state.onPhone}..remove(key)));
  }

  /// 스피커 키 바꾸기 (BLE). 성공하면 휴대폰에도 기억한다
  Future<Failure?> sendToSpeaker(String key, String value) async {
    emit(state.copyWith(sending: () => key));
    final r = await _repo.sendKeyOverBle(key, value);
    if (isClosed) return null;
    final failure = r.fold((f) => f, (_) => null);
    if (failure != null) {
      emit(state.copyWith(sending: () => null));
      return failure;
    }
    await _save(key, value);
    if (isClosed) return null;
    emit(state.copyWith(
      sending: () => null,
      onPhone: {...state.onPhone, key: value},
      onSpeaker: () => {...?state.onSpeaker, key: true},
    ));
    return null;
  }

  /// 스피커에서 지우기 (LAN). 휴대폰에 기억한 값은 그대로
  Future<Failure?> deleteOnSpeaker(String key) async {
    emit(state.copyWith(busy: true));
    final r = await _repo.deleteSpeakerKey(key);
    if (isClosed) return null;
    return r.fold((f) {
      emit(state.copyWith(busy: false));
      return f;
    }, (_) {
      emit(state.copyWith(busy: false, onSpeaker: () => {...?state.onSpeaker, key: false}));
      return null;
    });
  }

  Future<Map<String, String>> _readVault() async {
    final openAi = await _vault.openAiKey();
    final gemini = await _vault.geminiKey();
    final telegram = await _vault.telegramToken();
    return {
      if (openAi != null && openAi.isNotEmpty) SecretKey.openai: openAi,
      if (gemini != null && gemini.isNotEmpty) SecretKey.gemini: gemini,
      if (telegram != null && telegram.isNotEmpty) SecretKey.telegramBot: telegram,
    };
  }

  Future<void> _save(String key, String? value) => switch (key) {
        SecretKey.openai => _vault.saveOpenAiKey(value),
        SecretKey.gemini => _vault.saveGeminiKey(value),
        SecretKey.telegramBot => _vault.saveTelegramToken(value),
        _ => Future.value(),
      };
}
