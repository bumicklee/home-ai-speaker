/// 비서 기능 진입점. `injection_container.dart`에서 [registerAssistantFeature]를 부르고,
/// `app_router.dart`에서 `AppRoutes.assistant`를 [AssistantPage]에 연결한다.
library;

import 'package:get_it/get_it.dart';

import 'data/datasources/assistant_local_api_datasource.dart';
import 'data/repositories/assistant_repository_impl.dart';
import 'data/services/location_service.dart';
import 'domain/repositories/assistant_repository.dart';
import 'presentation/bloc/ai_mode_cubit.dart';
import 'presentation/bloc/assistant_cubit.dart';
import 'presentation/bloc/memories_cubit.dart';

export 'presentation/pages/ai_mode_page.dart' show AiModePage;
export 'presentation/pages/assistant_page.dart' show AssistantPage;
export 'presentation/pages/memories_page.dart' show MemoriesPage;

void registerAssistantFeature(GetIt sl) {
  sl
    ..registerLazySingleton<AssistantLocalApiDataSource>(() => AssistantLocalApiDataSource(sl()))
    ..registerLazySingleton<AssistantRepository>(() => AssistantRepositoryImpl(sl()))
    ..registerLazySingleton<LocationService>(DeviceLocationService.new)
    ..registerFactory<AssistantCubit>(() => AssistantCubit(sl(), sl()))
    // SystemRepository(키 상태·BLE 키 보내기)는 registerSystemFeature, KeyVault는 core에서 등록
    ..registerFactory<AiModeCubit>(() => AiModeCubit(sl(), sl(), sl()))
    ..registerFactory<MemoriesCubit>(() => MemoriesCubit(sl()));
}
