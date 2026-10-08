import 'package:flutter/foundation.dart';

import '../network/device_api_client.dart';
import '../network/device_endpoint.dart';
import '../services/device_event_service.dart';

/// 지금 선택된 스피커. 기능 화면의 데이터소스는 여기서 LAN 클라이언트를 얻는다.
/// DevicesCubit이 선택을 바꾸면 이벤트 WebSocket도 같이 옮긴다.
class CurrentDevice extends ChangeNotifier {
  CurrentDevice(this._clients, this._events);

  final DeviceApiClientFactory _clients;
  final DeviceEventService _events;
  DeviceEndpoint? _endpoint;
  void Function(DeviceEndpoint)? _onEndpointChanged;

  DeviceEndpoint? get endpoint => _endpoint;
  String? get deviceId => _endpoint?.deviceId;
  bool get hasDevice => _endpoint != null;

  /// 선택된 기기의 클라이언트. 없으면 StateError (화면은 기기가 있을 때만 열린다)
  DeviceApiClient get client {
    final e = _endpoint;
    if (e == null) throw StateError('no device selected');
    return _clients.forDevice(e, onEndpointChanged: _changed);
  }

  /// [onEndpointChanged]: 클라이언트가 mDNS로 새 IP를 찾았을 때 저장하라고 알린다
  void select(DeviceEndpoint? endpoint, {void Function(DeviceEndpoint)? onEndpointChanged}) {
    _onEndpointChanged = onEndpointChanged;
    if (endpoint == _endpoint) return;
    _endpoint = endpoint;
    _events.select(endpoint);
    notifyListeners();
  }

  void _changed(DeviceEndpoint e) {
    _endpoint = e;
    _events.select(e);
    _onEndpointChanged?.call(e);
    notifyListeners();
  }
}
