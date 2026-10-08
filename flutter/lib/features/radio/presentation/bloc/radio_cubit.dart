import 'dart:async';

import 'package:dartz/dartz.dart';
import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/failures.dart';
import '../../../../core/services/device_event_service.dart';
import '../../data/repositories/radio_repository.dart';
import '../../domain/entities/radio_preset.dart';
import '../../domain/radio_rules.dart';

class RadioState extends Equatable {
  const RadioState({
    this.presets = const [],
    this.now = const RadioNow(),
    this.loaded = false,
    this.loading = false,
    this.busy = false,
    this.failure,
    this.invalid,
  });

  final List<RadioPreset> presets;
  final RadioNow now;

  /// 프리셋을 한 번이라도 받았는지
  final bool loaded;
  final bool loading;

  /// 재생·저장 요청 중
  final bool busy;

  /// 마지막 불러오기·동작 실패 (화면은 바뀔 때 스낵바로)
  final Failure? failure;

  /// 보내기 전 검증 실패 (펌웨어 규칙)
  final PresetError? invalid;

  int? get playingIndex => playingPresetIndex(presets, now);

  RadioState copyWith({
    List<RadioPreset>? presets,
    RadioNow? now,
    bool? loaded,
    bool? loading,
    bool? busy,
    Failure? Function()? failure,
    PresetError? Function()? invalid,
  }) =>
      RadioState(
        presets: presets ?? this.presets,
        now: now ?? this.now,
        loaded: loaded ?? this.loaded,
        loading: loading ?? this.loading,
        busy: busy ?? this.busy,
        failure: failure != null ? failure() : this.failure,
        invalid: invalid != null ? invalid() : this.invalid,
      );

  @override
  List<Object?> get props => [presets, now, loaded, loading, busy, failure, invalid];
}

/// 라디오 화면: 프리셋 + 지금 재생 (WS `media_state`로 실시간 갱신)
class RadioCubit extends Cubit<RadioState> {
  RadioCubit(this._repo, this._events) : super(const RadioState()) {
    _sub = _events.on(DeviceEventType.mediaState).listen(_onMedia);
  }

  final RadioRepository _repo;
  final DeviceEventService _events;
  late final StreamSubscription<DeviceEvent> _sub;

  Future<void> load() async {
    emit(state.copyWith(loading: true, failure: () => null));
    final results = await Future.wait([_repo.presets(), _repo.now()]);
    if (isClosed) return;
    final presets = results[0] as Either<Failure, List<RadioPreset>>;
    final now = results[1] as Either<Failure, RadioNow>;
    Failure? failure;
    var next = state.copyWith(loading: false);
    presets.fold((f) => failure = f, (p) => next = next.copyWith(presets: p, loaded: true));
    now.fold((f) => failure ??= f, (n) => next = next.copyWith(now: n));
    emit(next.copyWith(failure: () => failure));
  }

  Future<void> playPreset(int index) => _act(() => _repo.playPreset(index));

  /// 검색 결과를 저장하지 않고 바로 튼다
  Future<void> playStation(RadioStation s) =>
      _act(() => _repo.playUrl(s.url, truncateUtf8(s.name, RadioLimits.nameMax)));

  /// 재생·연결 중이면 일시정지, 아니면 다시 재생
  Future<void> togglePause() => media(state.now.canPause ? 'pause' : 'resume');

  /// pause / resume / stop / next / previous. 409(재생 중 아님)면 지금 상태를 다시 받는다
  Future<void> media(String action) async {
    final ok = await _act(() => _repo.media(action));
    if (!ok && !isClosed && _isNotPlaying(state.failure)) await refreshNow();
  }

  Future<void> refreshNow() async {
    final r = await _repo.now();
    if (isClosed) return;
    r.fold((_) {}, (n) => emit(state.copyWith(now: n)));
  }

  /// 목록 전체를 바꾼다 (편집 화면의 저장). 보내기 전에 펌웨어와 같은 규칙으로 검증한다
  Future<bool> savePresets(List<RadioPreset> presets) async {
    final invalid = validatePresets(presets);
    if (invalid != null) {
      emit(state.copyWith(invalid: () => invalid, failure: () => null));
      return false;
    }
    emit(state.copyWith(busy: true, failure: () => null, invalid: () => null));
    final r = await _repo.savePresets(presets);
    if (isClosed) return r.isRight();
    return r.fold((f) {
      emit(state.copyWith(busy: false, failure: () => f));
      return false;
    }, (saved) {
      emit(state.copyWith(busy: false, presets: saved, loaded: true));
      return true;
    });
  }

  /// 맨 뒤에 추가 (같은 주소가 이미 있으면 거절)
  Future<bool> addPreset(RadioPreset preset) async {
    if (state.presets.any((p) => p.url == preset.url)) {
      emit(state.copyWith(invalid: () => PresetError.duplicate, failure: () => null));
      return false;
    }
    return savePresets([...state.presets, preset]);
  }

  bool hasUrl(String url) => state.presets.any((p) => p.url == url);

  Future<bool> _act(Future<Either<Failure, Unit>> Function() run) async {
    emit(state.copyWith(busy: true, failure: () => null, invalid: () => null));
    final r = await run();
    if (isClosed) return r.isRight();
    emit(state.copyWith(busy: false, failure: () => r.fold((f) => f, (_) => null)));
    return r.isRight();
  }

  void _onMedia(DeviceEvent e) {
    if (isClosed) return;
    emit(state.copyWith(now: RadioRepository.parseNow(e.data)));
  }

  static bool _isNotPlaying(Failure? f) => f is DeviceFailure && f.code == 'NOT_PLAYING';

  @override
  Future<void> close() async {
    await _sub.cancel();
    return super.close();
  }
}
