/// 시스템 기능 (설계서 2.13–2.14절): 기기 정보, 등록된 휴대폰, 다시 시작, 키 관리, 앱 설정.
/// `injection_container.dart`에서 registerSystemFeature(sl)를 부르고, `/system` 경로를 SystemPage로 연결한다.
library;

import 'package:get_it/get_it.dart';

import 'data/repositories/system_repository_impl.dart';
import 'domain/repositories/system_repository.dart';
import 'presentation/bloc/keys_cubit.dart';
import 'presentation/bloc/system_cubit.dart';

export 'presentation/pages/app_settings_page.dart' show AppSettingsPage;
export 'presentation/pages/keys_page.dart' show KeysPage;
export 'presentation/pages/system_page.dart' show SystemPage;

/// 필요: CurrentDevice, BleSpeakerDataSource, KeyVault (core에서 등록)
void registerSystemFeature(GetIt sl) {
  sl
    ..registerLazySingleton<SystemRepository>(() => SystemRepositoryImpl(sl(), sl()))
    ..registerFactory<SystemCubit>(() => SystemCubit(sl()))
    ..registerFactory<KeysCubit>(() => KeysCubit(sl(), sl()));
}
