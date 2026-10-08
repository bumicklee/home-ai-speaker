import 'dart:async';

import 'package:dartz/dartz.dart';
import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/ai/ai_choice.dart';
import '../../../../core/ble/ble_link.dart';
import '../../../../core/ble/ble_secret_writer.dart';
import '../../../../core/ble/wifi_scan.dart';
import '../../../../core/constants/api_paths.dart';
import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/errors/error_keys.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/storage/key_vault.dart';
import '../../../devices/domain/entities/device.dart';
import '../../domain/entities/provisioning_input.dart';
import '../../domain/repositories/provisioning_repository.dart';

enum ProvStep {
  keys, // 1. AI·키·방식 (고른 AI를 못 쓰는 스피커면 missingKey·notReady를 들고 여기로 돌아온다)
  scanning, // 2. 스피커 찾기
  connecting,
  wifiChoice, // 이미 Wi-Fi가 있는 스피커: 유지 / 바꾸기
  wifiScanning, // 3. Wi-Fi
  wifiList,
  wifiConnecting,
  sending, // 키 저장·휴대폰 등록
  phonesFull, // 등록 칸이 가득 참: 지울 휴대폰 고르기
  discovering, // 재부팅 뒤 LAN에서 찾기
  done, // 4. 완료
  failed,
}

class RegisteredPhone extends Equatable {
  const RegisteredPhone({required this.phoneId, required this.name});
  final String phoneId;
  final String name;
  @override
  List<Object?> get props => [phoneId, name];
}

class ProvisioningState extends Equatable {
  const ProvisioningState({
    this.step = ProvStep.keys,
    this.input = const ProvisioningInput(phoneName: ''),
    this.remember = true,
    this.speakers = const [],
    this.speaker,
    this.info,
    this.networks = const [],
    this.wifiErrorKey,
    this.lastSsid,
    this.phones = const [],
    this.failure,
    this.device,
    this.wifiChanged = false,
    this.done = const {},
    this.searching = false,
    this.missingKey,
    this.notReady,
  });

  final ProvStep step;
  final ProvisioningInput input;

  /// 키를 휴대폰에 기억 (사용자 요청 2026-10-03)
  final bool remember;
  final List<DiscoveredSpeaker> speakers;
  final DiscoveredSpeaker? speaker;
  final DeviceInfo? info;
  final List<WifiNetwork> networks;

  /// Wi-Fi 실패 이유 (ErrorKeys.wifi*)
  final String? wifiErrorKey;
  final String? lastSsid;
  final List<RegisteredPhone> phones;
  final Failure? failure;

  /// 완료 후 저장된 기기 (host가 null이면 LAN에서 아직 못 찾음)
  final Device? device;
  final bool wifiChanged;

  /// 완료 화면의 체크 목록: wifi, key, pair, found
  final Set<String> done;

  /// 스피커를 찾는 중 (30초 스캔이 끝나면 false)
  final bool searching;

  /// 연결해 보니 고른 AI의 키가 스피커에 없어 1단계로 돌아옴 (화면이 안내한다)
  final AiProvider? missingKey;

  /// 연결해 보니 이 스피커(펌웨어)가 고른 AI를 아직 못 함 (`device.info.ai.available`)
  final AiProvider? notReady;

  /// 단계 막대 (1–4)
  int get stepNumber => switch (step) {
        ProvStep.keys => 1,
        ProvStep.scanning || ProvStep.connecting => 2,
        ProvStep.wifiChoice || ProvStep.wifiScanning || ProvStep.wifiList || ProvStep.wifiConnecting => 3,
        _ => 4,
      };

  ProvisioningState copyWith({
    ProvStep? step,
    ProvisioningInput? input,
    bool? remember,
    List<DiscoveredSpeaker>? speakers,
    DiscoveredSpeaker? Function()? speaker,
    DeviceInfo? Function()? info,
    List<WifiNetwork>? networks,
    String? Function()? wifiErrorKey,
    String? Function()? lastSsid,
    List<RegisteredPhone>? phones,
    Failure? Function()? failure,
    Device? Function()? device,
    bool? wifiChanged,
    Set<String>? done,
    bool? searching,
    AiProvider? Function()? missingKey,
    AiProvider? Function()? notReady,
  }) =>
      ProvisioningState(
        step: step ?? this.step,
        input: input ?? this.input,
        remember: remember ?? this.remember,
        speakers: speakers ?? this.speakers,
        speaker: speaker != null ? speaker() : this.speaker,
        info: info != null ? info() : this.info,
        networks: networks ?? this.networks,
        wifiErrorKey: wifiErrorKey != null ? wifiErrorKey() : this.wifiErrorKey,
        lastSsid: lastSsid != null ? lastSsid() : this.lastSsid,
        phones: phones ?? this.phones,
        failure: failure != null ? failure() : this.failure,
        device: device != null ? device() : this.device,
        wifiChanged: wifiChanged ?? this.wifiChanged,
        done: done ?? this.done,
        searching: searching ?? this.searching,
        missingKey: missingKey != null ? missingKey() : this.missingKey,
        notReady: notReady != null ? notReady() : this.notReady,
      );

  @override
  List<Object?> get props => [
        step,
        input,
        remember,
        speakers,
        speaker,
        info,
        networks,
        wifiErrorKey,
        lastSsid,
        phones,
        failure,
        device,
        wifiChanged,
        done,
        searching,
        missingKey,
        notReady,
      ];
}

/// 첫 설정·휴대폰 추가·재등록 (설계서 3.4절)
class ProvisioningCubit extends Cubit<ProvisioningState> {
  ProvisioningCubit(this._repo, this._vault) : super(const ProvisioningState());

  final ProvisioningRepository _repo;
  final KeyVault _vault;
  StreamSubscription<List<DiscoveredSpeaker>>? _scan;

  /// 기억해 둔 키와 기본 휴대폰 이름으로 채운다
  Future<void> init() async {
    final openAi = await _vault.openAiKey() ?? '';
    final gemini = await _vault.geminiKey() ?? '';
    final tg = await _vault.telegramToken() ?? '';
    final phone = await _repo.defaultPhoneName();
    if (isClosed) return;
    emit(state.copyWith(
      input: ProvisioningInput(
        // OpenAI 키는 없고 Gemini 키만 기억해 뒀으면 Gemini를 먼저 고른다
        provider: openAi.isEmpty && gemini.isNotEmpty ? AiProvider.gemini : AiProvider.openai,
        openAiKey: openAi,
        geminiKey: gemini,
        telegramToken: tg,
        phoneName: phone,
      ),
    ));
  }

  /// 1단계 끝 → 스피커 찾기. 고른 AI의 키도 없고 "이미 있음"도 아니면 넘어가지 않는다 (false)
  Future<bool> submitKeys(ProvisioningInput input, {required bool remember}) async {
    if (!input.canSearch) return false;
    if (remember) {
      if (input.openAiKey.isNotEmpty) await _vault.saveOpenAiKey(input.openAiKey);
      if (input.geminiKey.isNotEmpty) await _vault.saveGeminiKey(input.geminiKey);
      if (input.telegramToken.isNotEmpty) await _vault.saveTelegramToken(input.telegramToken);
    } else {
      await _vault.saveOpenAiKey(null);
      await _vault.saveGeminiKey(null);
      await _vault.saveTelegramToken(null);
    }
    emit(state.copyWith(input: input, remember: remember, missingKey: () => null, notReady: () => null));
    await scan();
    return true;
  }

  /// 스피커 찾기 (30초). 이미 찾는 중이면 그대로 둔다 — 다시 시작하면 Android 스캔 횟수 제한에 걸린다
  Future<void> scan() async {
    if (_scan != null && state.step == ProvStep.scanning && state.searching) return;
    await _scan?.cancel();
    _scan = null;
    emit(state.copyWith(step: ProvStep.scanning, speakers: const [], searching: true, failure: () => null));
    final ready = await _repo.ensureReady();
    if (isClosed) return; // 기다리는 사이 화면이 닫힘
    if (_rightOrFail(ready) == null) return;
    _scan = _repo.scan().listen(
      (list) {
        if (!isClosed) emit(state.copyWith(speakers: list));
      },
      onError: (Object e) {
        if (!isClosed) _fail(const BleFailure(errorKey: ErrorKeys.bleProtocol));
      },
      onDone: () {
        _scan = null;
        if (!isClosed && state.step == ProvStep.scanning) emit(state.copyWith(searching: false));
      },
    );
  }

  Future<void> selectSpeaker(DiscoveredSpeaker speaker) async {
    await _scan?.cancel();
    _scan = null;
    emit(state.copyWith(step: ProvStep.connecting, speaker: () => speaker, failure: () => null));
    final info = _rightOrFail(await _repo.connect(speaker));
    if (info == null) return;
    emit(state.copyWith(info: () => info));
    final provider = state.input.provider;
    if (!info.ai.providerAvailable(provider)) {
      await _backToKeys(provider, notReady: true); // 예: 1단계 펌웨어에 Gemini
      return;
    }
    if (!info.ai.isAvailable(provider, state.input.mode)) {
      // 고른 방식을 이 스피커가 아직 못 하면 되는 방식으로 (나중에 비서 설정에서 바꿀 수 있다)
      final mode = AiMode.values.firstWhere((m) => info.ai.isAvailable(provider, m));
      emit(state.copyWith(input: state.input.copyWith(mode: mode)));
    }
    final onSpeaker = info.secrets[provider.secretKey] ?? false;
    if (!state.input.hasChosenKey && !onSpeaker) {
      await _backToKeys(provider); // 키를 넣거나 다른 AI를 고르게 1단계로
    } else {
      await _decideWifi();
    }
  }

  /// 고른 AI의 키가 스피커에 없거나([notReady]면) 이 스피커가 아직 못 하는 AI: 연결을 끊고 1단계로 (설계 11.3)
  Future<void> _backToKeys(AiProvider provider, {bool notReady = false}) async {
    await _repo.disconnect();
    emit(state.copyWith(
      step: ProvStep.keys,
      missingKey: () => notReady ? null : provider,
      notReady: () => notReady ? provider : null,
      input: state.input.copyWith(existingKey: false),
      speaker: () => null,
      info: () => null,
      done: const {},
    ));
  }

  Future<void> _decideWifi() async {
    final info = state.info;
    if (info != null && info.provisioned && info.wifiConnected) {
      emit(state.copyWith(step: ProvStep.wifiChoice));
    } else {
      await loadWifi();
    }
  }

  Future<void> keepWifi() => _complete();

  Future<void> loadWifi() async {
    emit(state.copyWith(step: ProvStep.wifiScanning, wifiErrorKey: () => null));
    final list = _rightOrFail(await _repo.scanWifi());
    if (list != null) emit(state.copyWith(step: ProvStep.wifiList, networks: list));
  }

  Future<void> connectWifi(String ssid, String password) async {
    emit(state.copyWith(step: ProvStep.wifiConnecting, lastSsid: () => ssid, wifiErrorKey: () => null));
    final status = _rightOrFail(await _repo.provisionWifi(ssid, password));
    if (status == null) return;
    if (status == WifiProvStatus.success) {
      emit(state.copyWith(wifiChanged: true, done: {...state.done, 'wifi'}));
      await _complete();
    } else {
      emit(state.copyWith(step: ProvStep.wifiList, wifiErrorKey: () => ErrorKeys.fromWifiStatus(status)));
    }
  }

  /// 키 저장 → 휴대폰 등록 → 0xFF88 → LAN에서 다시 찾기 → 휴대폰에 저장
  Future<void> _complete({String? evict}) async {
    emit(state.copyWith(step: ProvStep.sending));
    final input = state.input;
    if (evict == null) {
      if (input.openAiKey.isNotEmpty) {
        if (_rightOrFail(await _repo.setSecret(SecretKey.openai, input.openAiKey)) == null) return;
      }
      if (input.geminiKey.isNotEmpty) {
        if (_rightOrFail(await _repo.setSecret(SecretKey.gemini, input.geminiKey)) == null) return;
      }
      if (input.telegramToken.isNotEmpty) {
        if (_rightOrFail(await _repo.setSecret(SecretKey.telegramBot, input.telegramToken)) == null) return;
      }
      emit(state.copyWith(done: {...state.done, 'key'}));
    }
    final paired = await _repo.pair(input.phoneName, evict: evict);
    final pair = paired.fold<PairResult?>((f) {
      if (f is BleFailure && f.code == BleCmdError.phonesFull) {
        final phones = [
          for (final p in (f.data?['phones'] as List<dynamic>? ?? const []))
            RegisteredPhone(phoneId: p['phone_id'] as String, name: p['name'] as String? ?? ''),
        ];
        emit(state.copyWith(step: ProvStep.phonesFull, phones: phones));
      } else {
        _fail(f);
      }
      return null;
    }, (p) => p);
    if (pair == null) return;
    emit(state.copyWith(done: {...state.done, 'pair'}));

    // 고른 AI·방식 (스피커가 키를 한 번 더 확인한다)
    final ai = await _repo.setSettings(SettingsGroup.assistant, {
      'ai_provider': input.provider.id,
      input.provider.modeSettingKey: input.mode.id,
    });
    final aiFailure = ai.fold<Failure?>((f) => f, (_) => null);
    if (aiFailure != null) {
      if (aiFailure.code == BleCmdError.noApiKey) {
        await _backToKeys(input.provider);
      } else {
        _fail(aiFailure);
      }
      return;
    }

    if (_rightOrFail(await _repo.finish()) == null) return;
    final mac = await _macOrFail();
    if (mac == null) return;
    await _repo.disconnect();

    emit(state.copyWith(step: ProvStep.discovering));
    final name = state.speaker?.name ?? '${BleProtocol.namePrefix}${mac.substring(mac.length - 6)}';
    // 재부팅(Wi-Fi 바꿨을 때) 뒤 Wi-Fi 연결까지 수 초. 바꾸지 않았으면 바로 찾는다
    final device = await _repo.discover(mac, name);
    if (_rightOrFail(await _repo.save(device, pair.token)) == null) return;
    emit(state.copyWith(
      step: ProvStep.done,
      device: () => device,
      done: {...state.done, if (device.host != null) 'found'},
    ));
  }

  /// 등록 칸이 가득 찼을 때 지울 휴대폰을 고른다
  Future<void> evictAndPair(String phoneId) => _complete(evict: phoneId);

  /// 실패한 곳부터 다시: 연결 전이면 다시 찾기, 연결 뒤면 같은 스피커에 다시 연결
  Future<void> retry() async {
    final speaker = state.speaker;
    if (speaker == null) return scan();
    await selectSpeaker(speaker);
  }

  Future<String?> _macOrFail() async {
    final info = state.info;
    if (info == null || info.mac.isEmpty) {
      _fail(const BleFailure(errorKey: ErrorKeys.bleProtocol));
      return null;
    }
    return info.mac.toUpperCase();
  }

  /// 성공 값, 실패면 실패 화면으로 보내고 null
  T? _rightOrFail<T>(Either<Failure, T> r) => r.fold((f) {
        _fail(f);
        return null;
      }, (v) => v);

  @override
  void emit(ProvisioningState state) {
    if (!isClosed) super.emit(state); // BLE·네트워크 응답이 화면을 닫은 뒤 늦게 와도 무시
  }

  void _fail(Failure f) {
    if (isClosed) return;
    emit(state.copyWith(step: ProvStep.failed, failure: () => f));
  }

  @override
  Future<void> close() async {
    await _scan?.cancel();
    await _repo.disconnect();
    return super.close();
  }
}
