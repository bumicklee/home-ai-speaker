/// 기기 실시간 이벤트 (WS `/api/v1/events`, 설계서 2.14절). MqttService 대체.
/// 선택된 기기 하나에만 연결하고, 기기가 바뀌면 다시 연결한다. 끊기면 1초부터 최대 30초 간격으로 재연결.
library;

import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:equatable/equatable.dart';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import '../constants/api_paths.dart';
import '../network/device_endpoint.dart';

/// 이벤트 하나. [data]는 `event`를 뺀 나머지 필드
class DeviceEvent extends Equatable {
  const DeviceEvent(this.type, this.data);

  factory DeviceEvent.fromJson(Map<String, dynamic> j) =>
      DeviceEvent(j['event'] as String? ?? '', Map.of(j)..remove('event'));

  final String type;
  final Map<String, dynamic> data;

  @override
  List<Object?> get props => [type, data];
}

class DeviceEventType {
  DeviceEventType._();
  static const voiceState = 'voice_state';
  static const mediaState = 'media_state';
  static const volume = 'volume';
  static const tgMessage = 'tg_message';
  static const tgPaired = 'tg_paired';
  static const tgStatus = 'tg_status';
  static const wakeTest = 'wake_test';
  static const otaProgress = 'ota_progress';
  static const error = 'error';
}

enum EventConnection { disconnected, connecting, connected }

typedef WebSocketConnector = WebSocketChannel Function(Uri uri, Map<String, String> headers);

class DeviceEventService {
  DeviceEventService({WebSocketConnector? connector}) : _connect = connector ?? _defaultConnect;

  final WebSocketConnector _connect;
  final _events = StreamController<DeviceEvent>.broadcast();
  final _state = StreamController<EventConnection>.broadcast();

  DeviceEndpoint? _endpoint;
  WebSocketChannel? _channel;
  StreamSubscription<dynamic>? _sub;
  Timer? _retry;
  Duration _backoff = _minBackoff;
  EventConnection _connection = EventConnection.disconnected;

  static const _minBackoff = Duration(seconds: 1);
  static const _maxBackoff = Duration(seconds: 30);

  Stream<DeviceEvent> get events => _events.stream;
  Stream<DeviceEvent> on(String type) => events.where((e) => e.type == type);
  Stream<EventConnection> get connection => _state.stream;
  EventConnection get connectionNow => _connection;

  /// 기기를 바꾼다 (null이면 끊기만)
  void select(DeviceEndpoint? endpoint) {
    if (endpoint == _endpoint) return;
    _endpoint = endpoint;
    _backoff = _minBackoff;
    _close();
    if (endpoint != null) _open();
  }

  void _open() {
    final ep = _endpoint;
    if (ep == null) return;
    _setState(EventConnection.connecting);
    final uri = ep.baseUri.replace(scheme: 'ws', path: ApiPaths.events);
    // 토큰은 URL 쿼리가 아니라 헤더로
    final channel = _connect(uri, {'Authorization': 'Bearer ${ep.token}'});
    _channel = channel;
    channel.ready.then((_) {
      if (_channel != channel) return;
      _backoff = _minBackoff;
      _setState(EventConnection.connected);
    }, onError: (_) => _scheduleRetry(channel));
    _sub = channel.stream.listen(
      _onMessage,
      onError: (_) => _scheduleRetry(channel),
      onDone: () => _scheduleRetry(channel),
      cancelOnError: true,
    );
  }

  void _onMessage(dynamic message) {
    if (message is! String) return;
    try {
      final j = jsonDecode(message);
      if (j is Map<String, dynamic> && j['event'] is String) _events.add(DeviceEvent.fromJson(j));
    } on FormatException {
      // 깨진 메시지는 버린다
    }
  }

  void _scheduleRetry(WebSocketChannel channel) {
    if (_channel != channel) return; // 이미 바뀐 연결
    _close();
    _retry = Timer(_backoff, _open);
    _backoff = Duration(milliseconds: min(_backoff.inMilliseconds * 2, _maxBackoff.inMilliseconds));
  }

  void _close() {
    _retry?.cancel();
    _retry = null;
    _sub?.cancel();
    _sub = null;
    _channel?.sink.close();
    _channel = null;
    _setState(EventConnection.disconnected);
  }

  void _setState(EventConnection s) {
    if (_connection == s) return;
    _connection = s;
    _state.add(s);
  }

  Future<void> dispose() async {
    _endpoint = null;
    _close();
    await _events.close();
    await _state.close();
  }

  static WebSocketChannel _defaultConnect(Uri uri, Map<String, String> headers) =>
      IOWebSocketChannel.connect(uri, headers: headers, pingInterval: const Duration(seconds: 30));
}
