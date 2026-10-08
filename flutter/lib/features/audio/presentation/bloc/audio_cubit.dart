import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/failures.dart';
import '../../../../core/services/device_event_service.dart';
import '../../data/repositories/audio_repository.dart';
import '../../domain/entities/audio_settings.dart';

/// 호출어 감지 테스트 진행 상황
class WakeTest extends Equatable {
  const WakeTest({required this.remaining, this.detections = 0});

  /// 남은 초. 0이면 끝남 (결과 표시)
  final int remaining;
  final int detections;

  bool get running => remaining > 0;

  WakeTest copyWith({int? remaining, int? detections}) =>
      WakeTest(remaining: remaining ?? this.remaining, detections: detections ?? this.detections);

  @override
  List<Object?> get props => [remaining, detections];
}

class AudioState extends Equatable {
  const AudioState({
    this.audio,
    this.wake,
    this.ledBrightness,
    this.loading = false,
    this.failure,
    this.error,
    this.wakeTest,
  });

  final AudioSettings? audio;
  final WakeSettings? wake;
  final int? ledBrightness;
  final bool loading;

  /// 불러오기 실패 (화면 전체를 실패 화면으로)
  final Failure? failure;

  /// 저장·테스트 실패 (스낵바로 한 번 보여준다)
  final ActionError? error;
  final WakeTest? wakeTest;

  bool get loaded => audio != null && wake != null && ledBrightness != null;

  AudioState copyWith({
    AudioSettings? audio,
    WakeSettings? wake,
    int? ledBrightness,
    bool? loading,
    Failure? Function()? failure,
    ActionError? Function()? error,
    WakeTest? Function()? wakeTest,
  }) =>
      AudioState(
        audio: audio ?? this.audio,
        wake: wake ?? this.wake,
        ledBrightness: ledBrightness ?? this.ledBrightness,
        loading: loading ?? this.loading,
        failure: failure != null ? failure() : this.failure,
        error: error != null ? error() : this.error,
        wakeTest: wakeTest != null ? wakeTest() : this.wakeTest,
      );

  @override
  List<Object?> get props => [audio, wake, ledBrightness, loading, failure, error, wakeTest];
}

/// 오디오·호출어 화면: 설정 그룹 audio·wake·led + WS `volume`·`wake_test` (설계서 3.2절)
/// 슬라이더는 끄는 동안 [preview]로 화면만 바꾸고, 놓으면 바뀐 키 하나만 PUT 한다.
/// 저장이 실패하면 마지막으로 기기가 돌려준 값으로 되돌린다.
class AudioCubit extends Cubit<AudioState> {
  AudioCubit(this._repo, this._events, {Duration tick = const Duration(seconds: 1)})
      : _tick = tick,
        super(const AudioState()) {
    _volumeSub = _events.on(DeviceEventType.volume).listen(_onVolume);
    _wakeSub = _events.on(DeviceEventType.wakeTest).listen(_onWakeTest);
  }

  final AudioRepository _repo;
  final DeviceEventService _events;
  final Duration _tick;
  late final StreamSubscription<DeviceEvent> _volumeSub;
  late final StreamSubscription<DeviceEvent> _wakeSub;
  Timer? _timer;

  /// 기기가 마지막으로 확인해 준 값 (실패 시 되돌릴 곳)
  AudioSettings? _audio;
  WakeSettings? _wake;
  int? _led;

  /// 펌웨어 wake.c TEST_MS
  static const wakeTestSeconds = 60;

  Future<void> load() async {
    emit(state.copyWith(loading: true, failure: () => null));
    final (audio, wake, led) = await (_repo.audio(), _repo.wake(), _repo.ledBrightness()).wait;
    if (isClosed) return;
    Failure? failure;
    audio.fold((f) => failure ??= f, (v) => _audio = v);
    wake.fold((f) => failure ??= f, (v) => _wake = v);
    led.fold((f) => failure ??= f, (v) => _led = v);
    if (failure != null) {
      emit(state.copyWith(loading: false, failure: () => failure));
      return;
    }
    emit(state.copyWith(audio: _audio, wake: _wake, ledBrightness: _led, loading: false));
  }

  /// 슬라이더를 끄는 중: 화면만 바꾼다
  void preview({AudioSettings? audio, WakeSettings? wake, int? ledBrightness}) =>
      emit(state.copyWith(audio: audio, wake: wake, ledBrightness: ledBrightness));

  // ── audio ──

  Future<void> setVolume(int v) => _saveAudio(state.audio!.copyWith(volume: v), {'volume': v});
  Future<void> setDuck(int db) => _saveAudio(state.audio!.copyWith(duckDb: db), {'duck_db': db});
  Future<void> setAutoGain(bool on) => _saveAudio(state.audio!.copyWith(autoGain: on), {'auto_gain': on});

  Future<void> setMicGain(int db) {
    assert(AudioSettings.micGainSteps.contains(db));
    return _saveAudio(state.audio!.copyWith(micGain: db), {'mic_gain': db});
  }

  // ── wake ──

  Future<void> setWakeEnabled(bool on) => _saveWake(state.wake!.copyWith(enabled: on), {'enabled': on});
  Future<void> setPhrase(WakePhrase p) => _saveWake(state.wake!.copyWith(phrase: p), {'phrase': p.value});
  Future<void> setSensitivity(int v) => _saveWake(state.wake!.copyWith(sensitivity: v), {'sensitivity': v});
  Future<void> setWakeGain(int db) => _saveWake(state.wake!.copyWith(gainDb: db), {'gain_db': db});

  // ── led ──

  Future<void> setLedBrightness(int v) async {
    emit(state.copyWith(ledBrightness: v, error: () => null));
    final r = await _repo.setLedBrightness(v);
    if (isClosed) return;
    r.fold(
      (f) => emit(state.copyWith(ledBrightness: _led, error: () => ActionError(f))),
      (saved) => emit(state.copyWith(ledBrightness: _led = saved)),
    );
  }

  Future<void> testTone() async {
    final r = await _repo.testTone();
    if (isClosed) return;
    r.fold((f) => emit(state.copyWith(error: () => ActionError(f))), (_) {});
  }

  /// 60초 감지 테스트. 횟수는 `wake_test` 이벤트를 센다
  /// (이벤트의 `detections`는 부팅 후 누적이라 테스트 시작 기준이 아니다, wake.c)
  Future<void> startWakeTest() async {
    if (state.wakeTest?.running ?? false) return;
    emit(state.copyWith(error: () => null));
    final r = await _repo.startWakeTest();
    if (isClosed) return;
    r.fold((f) => emit(state.copyWith(error: () => ActionError(f))), (_) {
      emit(state.copyWith(wakeTest: () => const WakeTest(remaining: wakeTestSeconds)));
      _timer?.cancel();
      _timer = Timer.periodic(_tick, (_) => _countDown());
    });
  }

  void _countDown() {
    final t = state.wakeTest;
    if (t == null || !t.running) {
      _timer?.cancel();
      return;
    }
    final next = t.remaining - 1;
    if (next <= 0) _timer?.cancel();
    emit(state.copyWith(wakeTest: () => t.copyWith(remaining: next < 0 ? 0 : next)));
  }

  Future<void> _saveAudio(AudioSettings next, Map<String, Object> change) async {
    emit(state.copyWith(audio: next, error: () => null));
    final r = await _repo.updateAudio(change);
    if (isClosed) return;
    r.fold(
      (f) => emit(state.copyWith(audio: _audio, error: () => ActionError(f))),
      (saved) => emit(state.copyWith(audio: _audio = saved)),
    );
  }

  Future<void> _saveWake(WakeSettings next, Map<String, Object> change) async {
    emit(state.copyWith(wake: next, error: () => null));
    final r = await _repo.updateWake(change);
    if (isClosed) return;
    r.fold(
      (f) => emit(state.copyWith(wake: _wake, error: () => ActionError(f))),
      (saved) => emit(state.copyWith(wake: _wake = saved)),
    );
  }

  /// 버튼·음성으로 볼륨이 바뀌었을 때
  void _onVolume(DeviceEvent e) {
    final v = (e.data['value'] as num?)?.toInt();
    final a = _audio;
    if (v == null || a == null) return;
    _audio = a.copyWith(volume: v);
    emit(state.copyWith(audio: state.audio!.copyWith(volume: v)));
  }

  void _onWakeTest(DeviceEvent e) {
    final t = state.wakeTest;
    if (t == null || !t.running) return;
    // 설계서는 remaining_s도 보낸다고 하지만 지금 펌웨어는 detections만 보낸다. 오면 맞춘다
    final remaining = (e.data['remaining_s'] as num?)?.toInt();
    emit(state.copyWith(wakeTest: () => t.copyWith(detections: t.detections + 1, remaining: remaining)));
  }

  @override
  Future<void> close() async {
    _timer?.cancel();
    await _volumeSub.cancel();
    await _wakeSub.cancel();
    return super.close();
  }
}

/// 한 번 보여줄 실패. 같은 Failure가 연달아 나도 매번 새 값이라 리스너가 다시 울린다
class ActionError {
  ActionError(this.failure);
  final Failure failure;
}
