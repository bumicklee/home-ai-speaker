import 'package:dartz/dartz.dart';

import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../domain/entities/assistant_settings.dart';
import '../../domain/repositories/assistant_repository.dart';
import '../datasources/assistant_local_api_datasource.dart';
import '../models/assistant_settings_model.dart';

class AssistantRepositoryImpl implements AssistantRepository {
  AssistantRepositoryImpl(this._api);

  final AssistantLocalApiDataSource _api;

  @override
  Future<Either<Failure, AssistantSettings>> settings() =>
      guard(() async => AssistantSettingsModel.fromJson(await _api.settings()));

  @override
  Future<Either<Failure, AssistantSettings>> update(Map<String, dynamic> changes) =>
      guard(() async => AssistantSettingsModel.fromJson(await _api.update(changes)));

  @override
  Future<Either<Failure, List<String>>> voices() => guard(_api.voices);

  @override
  Future<Either<Failure, List<AssistantMemory>>> memories() => guard(() async {
        final m = await _api.memories();
        return [for (final e in m.entries) AssistantMemory(key: e.key, value: e.value)];
      });

  @override
  Future<Either<Failure, Unit>> forget(String key) => guard(() async {
        await _api.forget(key);
        return unit;
      });
}
