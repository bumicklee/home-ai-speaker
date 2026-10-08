import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/failures.dart';
import '../../data/models/assistant_settings_model.dart';
import '../../data/services/location_service.dart';
import '../../domain/entities/assistant_settings.dart';
import '../../domain/repositories/assistant_repository.dart';
import '../../domain/services/place_formatter.dart';

class AssistantState extends Equatable {
  const AssistantState({
    this.original,
    this.draft,
    this.voices = const [],
    this.memoryCount,
    this.loading = false,
    this.saving = false,
    this.locating = false,
    this.failure,
    this.timezone,
  });

  /// 기기에 저장된 값
  final AssistantSettings? original;

  /// 화면에서 고친 값 (저장 전)
  final AssistantSettings? draft;
  final List<String> voices;
  final int? memoryCount;
  final bool loading;
  final bool saving;
  final bool locating;

  /// 불러오기 실패 (화면 전체를 실패로)
  final Failure? failure;

  /// "현재 위치로" 뒤 휴대폰 시간대 판정. 스피커가 모르는 시간대면 화면이 후보를 보여준다
  final TimezoneSuggestion? timezone;

  bool get dirty => original != null && draft != null && original != draft;
  List<AssistantFieldError> get errors => draft?.validate() ?? const [];

  AssistantState copyWith({
    AssistantSettings? original,
    AssistantSettings? draft,
    List<String>? voices,
    int? Function()? memoryCount,
    bool? loading,
    bool? saving,
    bool? locating,
    Failure? Function()? failure,
    TimezoneSuggestion? Function()? timezone,
  }) =>
      AssistantState(
        original: original ?? this.original,
        draft: draft ?? this.draft,
        voices: voices ?? this.voices,
        memoryCount: memoryCount != null ? memoryCount() : this.memoryCount,
        loading: loading ?? this.loading,
        saving: saving ?? this.saving,
        locating: locating ?? this.locating,
        failure: failure != null ? failure() : this.failure,
        timezone: timezone != null ? timezone() : this.timezone,
      );

  @override
  List<Object?> get props => [original, draft, voices, memoryCount, loading, saving, locating, failure, timezone];
}

/// "현재 위치로" 결과 (화면이 안내 문구를 고른다)
enum LocateResult {
  /// 동네와 (스피커가 아는) 휴대폰 시간대를 넣음
  filled,

  /// 동네만 넣음. 시간대는 후보에서 골라야 한다
  filledTimezoneUnsupported,
  serviceOff,
  denied,
  deniedForever,
  unavailable,
}

/// 비서 설정: 불러오기 → 고치기(draft) → 바뀐 키만 저장
class AssistantCubit extends Cubit<AssistantState> {
  AssistantCubit(this._repo, this._location) : super(const AssistantState());

  final AssistantRepository _repo;
  final LocationService _location;

  Future<void> load() async {
    emit(state.copyWith(loading: true, failure: () => null));
    final settings = _repo.settings();
    final voices = _repo.voices();
    final memories = _repo.memories();
    final s = await settings;
    if (isClosed) return;
    await s.fold(
      (f) async => emit(state.copyWith(loading: false, failure: () => f)),
      (v) async {
        // 보이스·기억 수는 없어도 화면을 쓸 수 있다 (보이스는 저장된 것만 보인다)
        final names = (await voices).getOrElse(() => const []);
        final count = (await memories).fold((_) => null, (m) => m.length);
        if (isClosed) return;
        emit(AssistantState(original: v, draft: v, voices: names, memoryCount: count));
      },
    );
  }

  /// 기억 화면에서 돌아왔을 때
  Future<void> refreshMemoryCount() async {
    final r = await _repo.memories();
    if (isClosed) return;
    r.fold((_) {}, (m) => emit(state.copyWith(memoryCount: () => m.length)));
  }

  /// AI · 대화 방식 화면에서 돌아왔을 때: 그 화면이 바로 저장한 AI 값만 다시 읽는다 (고치던 다른 값은 그대로)
  Future<void> refreshAi() async {
    final r = await _repo.settings();
    if (isClosed) return;
    r.fold((_) {}, (v) {
      AssistantSettings withAi(AssistantSettings s) =>
          s.copyWith(aiProvider: v.aiProvider, modeOpenai: v.modeOpenai, modeGemini: v.modeGemini);
      final o = state.original;
      final d = state.draft;
      if (o == null || d == null) return;
      emit(state.copyWith(original: withAi(o), draft: withAi(d)));
    });
  }

  void edit(AssistantSettings Function(AssistantSettings) change) {
    final d = state.draft;
    if (d == null) return;
    emit(state.copyWith(draft: change(d)));
  }

  void setTimezone(String iana) {
    edit((d) => d.copyWith(timezone: iana));
    emit(state.copyWith(timezone: () => null));
  }

  /// 고친 것 버리기
  void discard() {
    final o = state.original;
    if (o != null) emit(state.copyWith(draft: o, timezone: () => null));
  }

  /// 바뀐 키만 PUT. 저장할 게 없거나 값이 틀리면 보내지 않는다. 실패하면 Failure (고친 값은 남긴다)
  Future<Failure?> save() async {
    final o = state.original;
    final d = state.draft;
    if (o == null || d == null || state.saving || state.errors.isNotEmpty) return null;
    final changes = AssistantSettingsModel.diff(o, d.copyWith(name: d.name.trim(), location: d.location.trim()));
    if (changes.isEmpty) {
      emit(state.copyWith(draft: o));
      return null;
    }
    emit(state.copyWith(saving: true));
    final r = await _repo.update(changes);
    if (isClosed) return r.fold((f) => f, (_) => null);
    return r.fold((f) {
      emit(state.copyWith(saving: false));
      return f;
    }, (saved) {
      emit(state.copyWith(saving: false, original: saved, draft: saved, timezone: () => null));
      return null;
    });
  }

  /// 휴대폰 위치 → 동네 이름(+ 시간대)을 draft에 넣는다. 저장은 사용자가 한다
  Future<LocateResult> locate() async {
    if (state.draft == null || state.locating) return LocateResult.unavailable;
    emit(state.copyWith(locating: true));
    try {
      final place = PlaceFormatter.format(await _location.currentPlace());
      if (place.isEmpty) throw const LocationLookupException(LocationError.unavailable);
      final tz = TimezoneSuggestion.of(await _location.phoneTimezone());
      if (isClosed) return LocateResult.filled;
      final d = state.draft!;
      emit(state.copyWith(
        locating: false,
        draft: d.copyWith(location: place, timezone: tz.supported ? tz.phone : null),
        timezone: () => tz.supported ? null : tz,
      ));
      return tz.supported ? LocateResult.filled : LocateResult.filledTimezoneUnsupported;
    } on LocationLookupException catch (e) {
      if (!isClosed) emit(state.copyWith(locating: false));
      return switch (e.error) {
        LocationError.serviceOff => LocateResult.serviceOff,
        LocationError.denied => LocateResult.denied,
        LocationError.deniedForever => LocateResult.deniedForever,
        LocationError.unavailable => LocateResult.unavailable,
      };
    }
  }

  Future<void> openLocationSettings() => _location.openSettings();
}
