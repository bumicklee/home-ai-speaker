import 'package:get_it/get_it.dart';

import 'data/repositories/lesson_repository.dart';
import 'presentation/bloc/english_cubit.dart';

export 'presentation/pages/english_page.dart' show EnglishPage;

/// 영어 선생님 (펌웨어 lesson.c, LAN /api/v1/lesson, WS `lesson`)
void registerEnglishFeature(GetIt sl) {
  sl
    ..registerLazySingleton<LessonRepository>(() => LessonRepository(sl(), sl(), sl()))
    ..registerFactory<EnglishCubit>(() => EnglishCubit(sl()));
}
