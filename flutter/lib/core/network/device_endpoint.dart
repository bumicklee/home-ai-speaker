import 'package:equatable/equatable.dart';

/// 기기 하나의 LAN 주소와 토큰
class DeviceEndpoint extends Equatable {
  const DeviceEndpoint({required this.deviceId, required this.host, this.port = 80, required this.token});

  /// STA MAC 12자 (0xFF61)
  final String deviceId;

  /// IP 주소 (mDNS로 찾은 값). `.local` 이름은 Android에서 해석이 불안정해 IP를 쓴다
  final String host;
  final int port;
  final String token;

  Uri get baseUri => Uri(scheme: 'http', host: host, port: port);

  DeviceEndpoint copyWith({String? host, int? port}) =>
      DeviceEndpoint(deviceId: deviceId, host: host ?? this.host, port: port ?? this.port, token: token);

  @override
  List<Object?> get props => [deviceId, host, port, token];
}

/// 연결이 안 될 때 mDNS로 IP를 다시 찾는다 (DeviceDiscoveryService가 구현)
abstract interface class DeviceResolver {
  Future<DeviceEndpoint?> resolve(DeviceEndpoint stale);
}
