/// 텔레그램 기능 입구. 경로 `/telegram` → [TelegramPage]
library;

import 'package:dio/dio.dart';
import 'package:get_it/get_it.dart';

import 'data/datasources/telegram_bot_api_datasource.dart';
import 'data/datasources/telegram_remote_datasource.dart';
import 'data/repositories/telegram_repository_impl.dart';
import 'domain/repositories/telegram_repository.dart';
import 'presentation/bloc/telegram_connect_cubit.dart';
import 'presentation/bloc/telegram_cubit.dart';

export 'presentation/pages/telegram_page.dart' show TelegramPage;

/// DataSource·Repository = LazySingleton, Cubit = Factory (설계서 3.2절).
/// 필요한 core: CurrentDevice, BleSpeakerDataSource, DeviceEventService, KeyVault, Dio('external')
void registerTelegramFeature(GetIt sl) {
  sl
    ..registerLazySingleton<TelegramRemoteDataSource>(() => TelegramRemoteDataSource(sl()))
    ..registerLazySingleton<TelegramBotApiDataSource>(() => TelegramBotApiDataSource(sl<Dio>(instanceName: 'external')))
    ..registerLazySingleton<TelegramRepository>(() => TelegramRepositoryImpl(sl(), sl(), sl(), sl(), sl()))
    ..registerFactory<TelegramCubit>(() => TelegramCubit(sl()))
    ..registerFactory<TelegramConnectCubit>(() => TelegramConnectCubit(sl(), sl()));
}
