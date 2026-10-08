import 'package:dartz/dartz.dart';

import '../../../../core/errors/failures.dart';
import '../../../../core/network/device_endpoint.dart';
import '../entities/device.dart';
import '../entities/device_status.dart';

abstract interface class DevicesRepository {
  Future<Either<Failure, List<Device>>> devices();

  /// 선택된 기기 id (없으면 첫 기기)
  Future<Either<Failure, String?>> selectedId();
  Future<Either<Failure, Unit>> select(String id);

  /// 등록·재등록: 목록에 넣거나 바꾸고 토큰을 저장한다. 프로비저닝은 이것 하나만 부른다 (설계서 3.2)
  Future<Either<Failure, Unit>> addOrUpdate(Device device, String token);

  /// 휴대폰에서만 지운다 (스피커 쪽 등록 해지는 system 기능)
  Future<Either<Failure, Unit>> remove(String id);

  /// LAN 주소. 토큰이나 주소가 없으면 null
  Future<Either<Failure, DeviceEndpoint?>> endpoint(String id);

  /// 주소를 모르거나 바뀌었으면 mDNS로 다시 찾아 저장
  Future<Either<Failure, Device>> rediscover(Device device);

  Future<Either<Failure, DeviceStatus>> status(DeviceEndpoint endpoint);
}
