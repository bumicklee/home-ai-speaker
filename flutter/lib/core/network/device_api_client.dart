/// 기기별 LAN API 클라이언트 (설계서 2.14, 3.2절).
/// baseUrl·토큰을 기기 단위로 묶는다. 토큰 갱신 인터셉터 없음 (401 = 해지).
/// 연결 실패면 mDNS로 IP를 다시 찾아 한 번 재시도한다.
library;

import 'dart:async';

import 'package:dio/dio.dart';

import '../errors/exceptions.dart';
import 'device_endpoint.dart';

class DeviceApiClient {
  DeviceApiClient(this._endpoint, {DeviceResolver? resolver, Dio? dio, this.onEndpointChanged})
      : _resolver = resolver,
        _dio = dio ?? Dio() {
    _dio.options
      ..connectTimeout = const Duration(seconds: 4)
      ..receiveTimeout = const Duration(seconds: 15)
      ..sendTimeout = const Duration(seconds: 15)
      ..responseType = ResponseType.json;
    _apply();
  }

  final Dio _dio;
  final DeviceResolver? _resolver;
  DeviceEndpoint _endpoint;

  /// IP가 바뀌었을 때 저장하라고 알린다
  final void Function(DeviceEndpoint)? onEndpointChanged;

  DeviceEndpoint get endpoint => _endpoint;

  void _apply() {
    _dio.options
      ..baseUrl = _endpoint.baseUri.toString()
      // Origin 헤더를 보내면 기기가 거부한다 (브라우저 차단용). Dio는 보내지 않는다
      ..headers = {'Authorization': 'Bearer ${_endpoint.token}'};
  }

  Future<T> get<T>(String path) => _run(() => _dio.get<T>(path));

  Future<T> put<T>(String path, Object? body) => _run(() => _dio.put<T>(path, data: body));

  Future<T> post<T>(String path, [Object? body]) => _run(() => _dio.post<T>(path, data: body));

  Future<T> delete<T>(String path) => _run(() => _dio.delete<T>(path));

  /// OTA 등 큰 업로드. Content-Length 필수 (기기가 크기를 먼저 확인)
  Future<T> upload<T>(String path, Stream<List<int>> data, int length,
          {void Function(int sent, int total)? onProgress, Duration timeout = const Duration(minutes: 5)}) =>
      _run(() => _dio.post<T>(path,
          data: data,
          options: Options(
            headers: {Headers.contentLengthHeader: length, Headers.contentTypeHeader: 'application/octet-stream'},
            sendTimeout: timeout,
            receiveTimeout: timeout,
          ),
          onSendProgress: onProgress),
          retry: false); // 스트림은 다시 보낼 수 없다

  Future<T> _run<T>(Future<Response<T>> Function() call, {bool retry = true}) async {
    try {
      return (await call()).data as T;
    } on DioException catch (e) {
      if (retry && _isUnreachable(e) && await _rediscover()) {
        try {
          return (await call()).data as T;
        } on DioException catch (e2) {
          throw _map(e2);
        }
      }
      throw _map(e);
    }
  }

  Future<bool> _rediscover() async {
    final fresh = await _resolver?.resolve(_endpoint);
    if (fresh == null || fresh == _endpoint) return false;
    _endpoint = fresh;
    _apply();
    onEndpointChanged?.call(fresh);
    return true;
  }

  static bool _isUnreachable(DioException e) =>
      e.type == DioExceptionType.connectionError || e.type == DioExceptionType.connectionTimeout;

  static AppException _map(DioException e) {
    final r = e.response;
    if (r != null) {
      final body = r.data;
      final code = body is Map ? body['error'] as String? : null;
      return DeviceApiException(r.statusCode ?? 0, code, e.message, body is Map<String, dynamic> ? body : null);
    }
    switch (e.type) {
      case DioExceptionType.connectionTimeout:
      case DioExceptionType.sendTimeout:
      case DioExceptionType.receiveTimeout:
        return DeviceTimeoutException(e.message);
      default:
        return NetworkException(e.message);
    }
  }
}

/// 기기마다 클라이언트 하나 (DI는 이 팩토리만 등록)
class DeviceApiClientFactory {
  DeviceApiClientFactory({this.resolver});

  final DeviceResolver? resolver;
  final _clients = <String, DeviceApiClient>{};

  DeviceApiClient forDevice(DeviceEndpoint endpoint, {void Function(DeviceEndpoint)? onEndpointChanged}) {
    final existing = _clients[endpoint.deviceId];
    if (existing != null && existing.endpoint.token == endpoint.token) return existing;
    return _clients[endpoint.deviceId] =
        DeviceApiClient(endpoint, resolver: resolver, onEndpointChanged: onEndpointChanged);
  }

  void remove(String deviceId) => _clients.remove(deviceId);
}
