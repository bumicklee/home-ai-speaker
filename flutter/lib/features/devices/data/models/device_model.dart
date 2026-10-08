import '../../domain/entities/device.dart';

/// SharedPreferences 저장 형식 (토큰은 여기 넣지 않는다. DeviceTokenStorage)
class DeviceModel {
  static Map<String, dynamic> toJson(Device d) => {'id': d.id, 'name': d.name, 'host': d.host, 'port': d.port};

  static Device fromJson(Map<String, dynamic> j) => Device(
        id: j['id'] as String,
        name: j['name'] as String? ?? j['id'] as String,
        host: j['host'] as String?,
        port: j['port'] as int? ?? 80,
      );
}
