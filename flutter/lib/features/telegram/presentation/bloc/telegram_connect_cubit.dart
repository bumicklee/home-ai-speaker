import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/failures.dart';
import '../../../../core/storage/key_vault.dart';
import '../../domain/entities/telegram.dart';
import '../../domain/repositories/telegram_repository.dart';

enum TokenCheck {
  idle,
  checking,
  valid, // getMe 성공
  invalid, // Telegram이 거절 (또는 모양이 틀림)
  failed, // Telegram에 닿지 못함
}

enum TokenSave { idle, saving, saved, failed }

enum Pairing {
  idle,
  requesting,
  waiting, // 코드 표시, tg_paired 기다림
  expired,
  paired,
  noToken, // 409: 스피커에 토큰이 없음
  failed,
}

class TelegramConnectState extends Equatable {
  const TelegramConnectState({
    this.token = '',
    this.check = TokenCheck.idle,
    this.botName = '',
    this.save = TokenSave.idle,
    this.pairing = Pairing.idle,
    this.code,
    this.remaining = Duration.zero,
    this.pairedChat,
    this.failure,
  });

  final String token;
  final TokenCheck check;

  /// getMe로 확인한 봇 이름 (@ 없이)
  final String botName;
  final TokenSave save;
  final Pairing pairing;
  final PairingCode? code;
  final Duration remaining;
  final PairedChat? pairedChat;

  /// 저장·코드 받기 실패 (BLE·LAN)
  final Failure? failure;

  /// t.me 링크와 그룹 명령에 쓰는 봇 이름 (스피커가 아직 모르면 앱이 확인한 이름)
  String get bot => (code?.bot.isNotEmpty ?? false) ? code!.bot : botName;

  TelegramConnectState copyWith({
    String? token,
    TokenCheck? check,
    String? botName,
    TokenSave? save,
    Pairing? pairing,
    PairingCode? Function()? code,
    Duration? remaining,
    PairedChat? Function()? pairedChat,
    Failure? Function()? failure,
  }) =>
      TelegramConnectState(
        token: token ?? this.token,
        check: check ?? this.check,
        botName: botName ?? this.botName,
        save: save ?? this.save,
        pairing: pairing ?? this.pairing,
        code: code != null ? code() : this.code,
        remaining: remaining ?? this.remaining,
        pairedChat: pairedChat != null ? pairedChat() : this.pairedChat,
        failure: failure != null ? failure() : this.failure,
      );

  @override
  List<Object?> get props => [token, check, botName, save, pairing, code, remaining, pairedChat, failure];
}

/// 텔레그램 연결: 토큰 확인(getMe) → BLE로 스피커에 저장 → 등록 코드 → `tg_paired` 기다림 (설계서 3.4·3.5절)
class TelegramConnectCubit extends Cubit<TelegramConnectState> {
  TelegramConnectCubit(this._repo, this._vault, {DateTime Function()? clock})
      : _now = clock ?? DateTime.now,
        super(const TelegramConnectState()) {
    _pairedSub = _repo.paired().listen(_onPaired);
  }

  final TelegramRepository _repo;
  final KeyVault _vault;
  final DateTime Function() _now;
  late final StreamSubscription<PairedChat> _pairedSub;
  Timer? _ticker;
  DateTime? _expiresAt;

  /// 휴대폰에 기억해 둔 토큰으로 채운다
  Future<void> init() async {
    final saved = await _vault.telegramToken() ?? '';
    if (isClosed || saved.isEmpty || state.token.isNotEmpty) return;
    emit(state.copyWith(token: saved));
  }

  void tokenChanged(String value) {
    final token = value.trim();
    if (token == state.token) return;
    emit(state.copyWith(token: token, check: TokenCheck.idle, botName: '', save: TokenSave.idle, failure: () => null));
  }

  /// 맞으면 휴대폰에 기억한다 (사용자 요청 2026-10-03)
  Future<void> checkToken() async {
    final token = state.token;
    if (token.isEmpty) return;
    emit(state.copyWith(check: TokenCheck.checking, failure: () => null));
    final r = await _repo.checkToken(token);
    if (isClosed || token != state.token) return;
    final name = r.fold((_) => null, (v) => v);
    if (r.isLeft()) {
      emit(state.copyWith(check: TokenCheck.failed));
    } else if (name == null) {
      emit(state.copyWith(check: TokenCheck.invalid));
    } else {
      await _vault.saveTelegramToken(token);
      emit(state.copyWith(check: TokenCheck.valid, botName: name));
    }
  }

  /// 사용자가 Key2를 3초 누른 뒤. [pairAfter]면 저장하자마자 등록 코드를 받는다
  Future<void> saveToSpeaker({bool pairAfter = true}) async {
    if (state.check != TokenCheck.valid || state.save == TokenSave.saving) return;
    emit(state.copyWith(save: TokenSave.saving, failure: () => null));
    final r = await _repo.saveTokenToSpeaker(state.token);
    if (isClosed) return;
    final ok = r.fold((f) {
      emit(state.copyWith(save: TokenSave.failed, failure: () => f));
      return false;
    }, (_) => true);
    if (!ok) return;
    emit(state.copyWith(save: TokenSave.saved));
    if (pairAfter) await startPairing();
  }

  /// 새 코드 (5분, 1회용). 다시 부르면 이전 코드는 무효
  Future<void> startPairing() async {
    _ticker?.cancel();
    emit(state.copyWith(pairing: Pairing.requesting, code: () => null, pairedChat: () => null, failure: () => null));
    final r = await _repo.startPairing();
    if (isClosed) return;
    r.fold((f) {
      final noToken = f is DeviceFailure && (f.status == 409 || f.code == 'NO_TOKEN');
      emit(state.copyWith(pairing: noToken ? Pairing.noToken : Pairing.failed, failure: () => f));
    }, (code) {
      _expiresAt = _now().add(code.expiresIn);
      emit(state.copyWith(pairing: Pairing.waiting, code: () => code, remaining: code.expiresIn));
      _ticker = Timer.periodic(const Duration(seconds: 1), (_) => tick());
    });
  }

  /// 1초마다 남은 시간을 줄이고, 다 되면 만료로 (테스트에서 직접 부른다)
  @visibleForTesting
  void tick() {
    final end = _expiresAt;
    if (end == null || isClosed || state.pairing != Pairing.waiting) return;
    final left = end.difference(_now());
    if (left <= Duration.zero) {
      _ticker?.cancel();
      emit(state.copyWith(pairing: Pairing.expired, remaining: Duration.zero));
    } else {
      emit(state.copyWith(remaining: left));
    }
  }

  void _onPaired(PairedChat chat) {
    if (state.pairing != Pairing.waiting) return;
    _ticker?.cancel();
    emit(state.copyWith(pairing: Pairing.paired, pairedChat: () => chat));
  }

  @override
  Future<void> close() async {
    _ticker?.cancel();
    await _pairedSub.cancel();
    return super.close();
  }
}
