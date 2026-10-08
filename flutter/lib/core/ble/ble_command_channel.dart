/// 0xFF81(요청)/0xFF82(응답) JSON 명령 채널.
/// 연결당 요청 1개씩 (기기는 동시 요청에 BUSY), 요청당 10초 (설계서 2.13절).
library;

import 'dart:async';
import 'dart:convert';

import '../constants/ble_protocol.dart';
import '../errors/exceptions.dart';
import 'ble_frame.dart';

/// 채널이 쓰는 BLE 연결 (실제 구현은 `BleLink`, 테스트는 가짜)
abstract interface class BleCommandTransport {
  /// 조각 payload 크기 = 지금 MTU − 5
  int get payloadSize;

  /// 0xFF82 알림 (구독은 연결 시 이미 켜져 있어야 한다)
  Stream<List<int>> get responses;

  /// 0xFF81에 응답 있는 쓰기
  Future<void> writeFrame(List<int> frame);
}

class BleCommandChannel {
  BleCommandChannel(this._transport, {this.timeout = BleProtocol.cmdTimeout}) {
    _sub = _transport.responses.listen(_onFrame, onError: _fail);
  }

  final BleCommandTransport _transport;
  final Duration timeout;
  final _reassembler = BleFrameReassembler();
  late final StreamSubscription<List<int>> _sub;

  int _nextId = 1;
  Future<void> _queue = Future.value();
  Completer<Map<String, dynamic>>? _pending;
  int? _pendingId;

  /// [op] 요청을 보내고 `ok: true` 응답을 돌려준다. `ok: false`면 [BleCommandException]
  Future<Map<String, dynamic>> request(String op, [Map<String, Object?> fields = const {}]) {
    final result = _queue.then((_) => _send(op, fields));
    _queue = result.then((_) {}, onError: (_) {});
    return result;
  }

  Future<Map<String, dynamic>> _send(String op, Map<String, Object?> fields) async {
    final id = _nextId++;
    final body = utf8.encode(jsonEncode({'id': id, 'op': op, ...fields}));
    final frames = BleFrameCodec.encode(body, _transport.payloadSize);
    final completer = Completer<Map<String, dynamic>>();
    _pending = completer;
    _pendingId = id;
    _reassembler.reset();
    try {
      for (final f in frames) {
        await _transport.writeFrame(f);
      }
      final response = await completer.future.timeout(
        timeout,
        onTimeout: () => throw BleException(BleErrorCode.timeout, '$op timed out'),
      );
      if (response['ok'] != true) {
        throw BleCommandException(response['error'] as String? ?? BleCmdError.invalidArg,
            data: response, message: op);
      }
      return response;
    } finally {
      _pending = null;
      _pendingId = null;
    }
  }

  void _onFrame(List<int> frame) {
    try {
      final message = _reassembler.add(frame);
      if (message == null) return;
      final decoded = jsonDecode(utf8.decode(message));
      if (decoded is! Map<String, dynamic>) {
        throw const BleException(BleErrorCode.protocol, 'response is not an object');
      }
      // 이전 요청(시간 초과로 포기한 것)의 늦은 응답은 버린다
      if (decoded['id'] == _pendingId) {
        _pending?.complete(decoded);
      }
    } on FormatException catch (e) {
      _fail(BleException(BleErrorCode.protocol, e.message));
    } catch (e) {
      _fail(e);
    }
  }

  void _fail(Object error) {
    final p = _pending;
    if (p != null && !p.isCompleted) {
      p.completeError(error is AppException ? error : BleException(BleErrorCode.protocol, '$error'));
    }
  }

  /// 연결이 끊겼을 때: 기다리는 요청을 실패시키고 구독을 정리한다
  Future<void> close() async {
    _fail(const BleException(BleErrorCode.disconnected));
    await _sub.cancel();
  }
}
