import 'package:equatable/equatable.dart';

import 'error_keys.dart';

/// 레포지토리가 돌려주는 실패. 화면은 [errorKey]로 현지화된 문구를 고른다.
sealed class Failure extends Equatable {
  const Failure({required this.errorKey, this.message, this.code});

  final String errorKey;

  /// 로그용 원문 (화면에 그대로 보이지 않는다)
  final String? message;
  final String? code;

  @override
  List<Object?> get props => [runtimeType, errorKey, message, code];
}

class DeviceFailure extends Failure {
  const DeviceFailure({required super.errorKey, this.status, super.message, super.code});

  factory DeviceFailure.fromStatus(int status, String? code, [String? message]) =>
      DeviceFailure(errorKey: ErrorKeys.fromDeviceStatus(status, code), status: status, code: code, message: message);

  const DeviceFailure.timeout() : this(errorKey: ErrorKeys.deviceTimeout);

  final int? status;

  /// 401 = 이 휴대폰 등록이 해지됨 → BLE 재등록 화면
  bool get isUnauthorized => status == 401;

  @override
  List<Object?> get props => [...super.props, status];
}

class BleFailure extends Failure {
  const BleFailure({required super.errorKey, super.message, super.code, this.data});

  /// 기기 오류 응답 전체 (`PHONES_FULL`의 `phones` 등)
  final Map<String, dynamic>? data;

  @override
  List<Object?> get props => [...super.props, data];
}

class NetworkFailure extends Failure {
  const NetworkFailure({super.message}) : super(errorKey: ErrorKeys.deviceUnreachable);
}

class CacheFailure extends Failure {
  const CacheFailure({super.message}) : super(errorKey: ErrorKeys.cache);
}

class UnknownFailure extends Failure {
  const UnknownFailure({super.message}) : super(errorKey: ErrorKeys.unknown);
}
