/// 오디오·호출어 기능 (경로 `/audio`). injection_container에서 [registerAudioFeature]를,
/// app_router에서 [AudioPage]를 연결한다.
library;

import 'package:get_it/get_it.dart';

import 'data/repositories/audio_repository.dart';
import 'presentation/bloc/audio_cubit.dart';

export 'presentation/pages/audio_page.dart' show AudioPage;

void registerAudioFeature(GetIt sl) {
  sl
    ..registerLazySingleton<AudioRepository>(() => AudioRepository(sl()))
    ..registerFactory<AudioCubit>(() => AudioCubit(sl(), sl()));
}
