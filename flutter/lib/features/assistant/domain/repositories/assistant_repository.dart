import 'package:dartz/dartz.dart';

import '../../../../core/errors/failures.dart';
import '../entities/assistant_settings.dart';

/// 선택된 스피커의 비서 설정·보이스·기억 (LAN)
abstract class AssistantRepository {
  Future<Either<Failure, AssistantSettings>> settings();

  /// 부분 수정: [changes]에 있는 키만 보낸다. 기기가 저장한 전체 설정을 돌려준다
  Future<Either<Failure, AssistantSettings>> update(Map<String, dynamic> changes);

  Future<Either<Failure, List<String>>> voices();

  Future<Either<Failure, List<AssistantMemory>>> memories();

  Future<Either<Failure, Unit>> forget(String key);
}
