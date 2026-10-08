import 'package:get_it/get_it.dart';

import 'data/datasources/notes_local_store.dart';
import 'data/datasources/notes_remote_datasource.dart';
import 'data/repositories/notes_repository.dart';
import 'presentation/bloc/notes_cubit.dart';

export 'presentation/pages/notes_page.dart' show NotesPage;

/// 메모·일정 (펌웨어 notes 모듈, LAN /api/v1/notes, WS `notes`·`reminder`)
void registerNotesFeature(GetIt sl) {
  sl
    ..registerLazySingleton<NotesRemoteDataSource>(() => NotesRemoteDataSource(sl()))
    ..registerLazySingleton<NotesLocalStore>(NotesLocalStore.new)
    ..registerLazySingleton<NotesRepository>(() => NotesRepository(sl(), sl(), sl(), sl()))
    ..registerFactory<NotesCubit>(() => NotesCubit(sl()));
}
