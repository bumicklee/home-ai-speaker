import 'package:equatable/equatable.dart';

/// 이 휴대폰에 등록된 스피커 하나
class Device extends Equatable {
  const Device({required this.id, required this.name, this.host, this.port = 80});

  /// STA MAC 12자 대문자 (BLE 0xFF61). 광고 이름 뒤 6자리·mDNS TXT `id`와 같은 값에서 나온다
  final String id;

  /// 화면에 보이는 이름 (처음엔 광고 이름 `JARVIS_XXXXXX`)
  final String name;

  /// 마지막으로 찾은 IP. 등록 직후 mDNS로 못 찾았으면 null (홈에서 다시 찾는다)
  final String? host;
  final int port;

  String get shortId => id.length >= 6 ? id.substring(id.length - 6) : id;

  Device copyWith({String? name, String? Function()? host, int? port}) =>
      Device(id: id, name: name ?? this.name, host: host != null ? host() : this.host, port: port ?? this.port);

  @override
  List<Object?> get props => [id, name, host, port];
}
