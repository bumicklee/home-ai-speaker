/// 데이터소스가 던지는 예외. 외부 라이브러리 예외(`DioException`, `FlutterBluePlusException`)는
/// 데이터소스에서 이것들로 바꾼다. `guard`는 이것만 다룬다.
library;

sealed class AppException implements Exception {
  const AppException([this.message]);
  final String? message;

  @override
  String toString() => '$runtimeType(${message ?? ''})';
}

/// LAN API가 HTTP 오류를 돌려줌. [code]는 본문 `{"error": "..."}` (없으면 null)
class DeviceApiException extends AppException {
  const DeviceApiException(this.status, this.code, [super.message, this.data]);
  final int status;
  final String? code;

  /// 오류 응답 본문 전체 (메모 409 `CONFLICT`의 `note` 등). 본문이 JSON 객체가 아니면 null
  final Map<String, dynamic>? data;
}

/// 기기 응답 시간 초과. dart:async `TimeoutException`과 이름이 겹치지 않게 따로 둔다
class DeviceTimeoutException extends AppException {
  const DeviceTimeoutException([super.message]);
}

/// 기기에 닿지 못함 (같은 Wi-Fi가 아님, IP 바뀜, 꺼짐)
class NetworkException extends AppException {
  const NetworkException([super.message]);
}

/// BLE 오류. [code]는 `BleErrorCode` 값
class BleException extends AppException {
  const BleException(this.code, [super.message]);
  final String code;
}

/// 0xFF81 명령에 기기가 `ok: false`로 답함. [code]는 `BleCmdError` 값
class BleCommandException extends AppException {
  const BleCommandException(this.code, {this.data, String? message}) : super(message);
  final String code;

  /// 오류 응답 전체 (`PHONES_FULL`의 `phones` 목록 등)
  final Map<String, dynamic>? data;
}

class CacheException extends AppException {
  const CacheException([super.message]);
}

/// 앱 내부 BLE 오류 코드
class BleErrorCode {
  BleErrorCode._();
  static const adapterOff = 'adapter_off';
  static const permissionDenied = 'permission_denied';
  static const notFound = 'not_found';
  static const connectFailed = 'connect_failed';
  static const disconnected = 'disconnected';
  static const serviceMissing = 'service_missing';
  static const bondFailed = 'bond_failed';
  static const protocol = 'protocol';
  static const tooLarge = 'too_large';
  static const timeout = 'timeout';
}
