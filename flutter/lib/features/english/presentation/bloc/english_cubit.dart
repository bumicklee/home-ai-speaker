import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/error_keys.dart';
import '../../../../core/errors/failures.dart';
import '../../data/repositories/lesson_repository.dart';
import '../../domain/entities/lesson.dart';

class EnglishState extends Equatable {
  const EnglishState({
    this.options = const LessonOptions(),
    this.starting = false,
    this.started,
    this.failure,
    this.openAiKey,
  });

  final LessonOptions options;
  final bool starting;

  /// 시작에 성공하면 그 수업 (화면이 수업 화면으로 넘어간다)
  final Lesson? started;
  final Failure? failure;

  /// 스피커에 OpenAI 키가 있는지. 모르면 null (시작은 막지 않는다. 스피커가 409로 막는다)
  final bool? openAiKey;

  /// 키가 없다고 확인되면 시작 버튼을 잠근다
  bool get canStart => !starting && openAiKey != false;

  EnglishState copyWith({
    LessonOptions? options,
    bool? starting,
    Lesson? Function()? started,
    Failure? Function()? failure,
    bool? Function()? openAiKey,
  }) =>
      EnglishState(
        options: options ?? this.options,
        starting: starting ?? this.starting,
        started: started != null ? started() : this.started,
        failure: failure != null ? failure() : this.failure,
        openAiKey: openAiKey != null ? openAiKey() : this.openAiKey,
      );

  @override
  List<Object?> get props => [options, starting, started, failure, openAiKey];
}

/// 코스 고르기 (디자인 "영어 선생님: 코스 고르기")
class EnglishCubit extends Cubit<EnglishState> {
  EnglishCubit(this._repo) : super(EnglishState(options: _repo.savedOptions()));

  final LessonRepository _repo;

  /// 화면을 열 때: 스피커에 OpenAI 키가 있는지 (못 읽으면 모르는 채로 둔다)
  Future<void> checkKey() async {
    final r = await _repo.openAiKeySaved();
    if (isClosed) return;
    r.fold((_) {}, (has) => emit(state.copyWith(openAiKey: () => has)));
  }

  void select(LessonOptions options) => emit(state.copyWith(options: options, failure: () => null));

  Future<void> start() async {
    emit(state.copyWith(starting: true, failure: () => null, started: () => null));
    await _repo.saveOptions(state.options);
    final r = await _repo.start(state.options);
    if (isClosed) return;
    r.fold(
      (f) => emit(state.copyWith(
            starting: false,
            failure: () => f,
            // 409 NO_API_KEY: 그사이 키가 지워졌다 → 시작을 잠그고 안내
            openAiKey: f.errorKey == ErrorKeys.deviceNoApiKey ? () => false : null,
          )),
      (lesson) => emit(state.copyWith(starting: false, started: () => lesson)),
    );
  }

  /// 수업 화면으로 넘어간 뒤 다시 누를 수 있게
  void consumeStarted() => emit(state.copyWith(started: () => null));
}
