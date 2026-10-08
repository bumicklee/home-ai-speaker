/// 스피커 BLE 연결 하나 (설계서 2.13, 3.4절).
/// 검색(withServices FF50 + advName 접두어) → 연결 → MTU → 특성 탐색(Guid 비교) → **알림 먼저 구독** → 0xFF61 읽기.
library;

import 'dart:async';
import 'dart:convert';
import 'dart:io' show Platform;

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../constants/ble_protocol.dart';
import '../errors/exceptions.dart';
import 'ble_command_channel.dart';
import 'wifi_scan.dart';

/// 검색으로 찾은 스피커
class DiscoveredSpeaker {
  const DiscoveredSpeaker({required this.device, required this.name, required this.rssi});

  final BluetoothDevice device;

  /// 광고 이름 `JARVIS_XXXXXX` (iOS가 캐시하는 platformName은 쓰지 않는다)
  final String name;
  final int rssi;

  /// 광고 이름 뒤 6자리 = STA MAC 뒤 6자리
  String get shortId => name.substring(BleProtocol.namePrefix.length);
}

class BleScanner {
  /// BLE가 켜질 때까지 기다린다. 꺼져 있으면 [BleException] adapterOff
  static Future<void> ensureOn({Duration wait = const Duration(seconds: 3)}) async {
    if (!await FlutterBluePlus.isSupported) {
      throw const BleException(BleErrorCode.adapterOff, 'BLE not supported');
    }
    try {
      await FlutterBluePlus.adapterState
          .firstWhere((s) => s == BluetoothAdapterState.on)
          .timeout(wait);
    } on TimeoutException {
      throw const BleException(BleErrorCode.adapterOff);
    }
  }

  /// 주변 스피커 목록 (신호 센 순). [timeout]이 지나면 스트림이 끝난다.
  ///
  /// 스캔 시작·정지는 휴대폰 전체에 하나뿐이다. 그래서 정지는 **이 스캔을 시작한 구독이 취소될 때만**,
  /// 그리고 그 사이 다른 스캔이 시작되지 않았을 때만 한다 (2026-10-03 실기기: 닫힌 화면에 남은 스캔이
  /// 새 화면의 스캔을 곧바로 멈춰 스피커를 못 찾음).
  /// Android는 30초에 5번 넘게 시작하면 조용히 거절한다(status=6) → [_waitForScanSlot]으로 미리 기다린다.
  static Stream<List<DiscoveredSpeaker>> scan({Duration timeout = const Duration(seconds: 30)}) {
    final owner = Object();
    StreamSubscription<List<ScanResult>>? results;
    StreamSubscription<bool>? scanning;
    late final StreamController<List<DiscoveredSpeaker>> controller;

    Future<void> finish() async {
      await results?.cancel();
      await scanning?.cancel();
      if (!controller.isClosed) await controller.close();
    }

    controller = StreamController<List<DiscoveredSpeaker>>(
      onListen: () async {
        try {
          await ensureOn();
          await _waitForScanSlot();
          if (controller.isClosed || !controller.hasListener) return;
          results = FlutterBluePlus.onScanResults.listen((list) {
            if (controller.isClosed) return;
            controller.add([
              for (final r in list)
                if (r.advertisementData.advName.startsWith(BleProtocol.namePrefix))
                  DiscoveredSpeaker(device: r.device, name: r.advertisementData.advName, rssi: r.rssi),
            ]..sort((a, b) => b.rssi.compareTo(a.rssi)));
          });
          var started = false;
          scanning = FlutterBluePlus.isScanning.listen((on) {
            if (on) {
              started = true;
            } else if (started) {
              finish(); // 시간이 다 됐거나 OS가 멈춤
            }
          });
          _owner = owner;
          _starts.add(DateTime.now());
          await FlutterBluePlus.startScan(withServices: [BleProtocol.svcWifi], timeout: timeout);
        } on AppException catch (e) {
          if (!controller.isClosed) controller.addError(e);
          await finish();
        } on FlutterBluePlusException catch (e) {
          if (!controller.isClosed) controller.addError(BleException(BleErrorCode.protocol, e.description));
          await finish();
        }
      },
      onCancel: () async {
        await results?.cancel();
        await scanning?.cancel();
        if (identical(_owner, owner)) {
          _owner = null;
          if (FlutterBluePlus.isScanningNow) await FlutterBluePlus.stopScan();
        }
      },
    );
    return controller.stream;
  }

  static Object? _owner;
  static final _starts = <DateTime>[];

  /// 최근 30초 안에 시작이 4번이면 가장 오래된 것이 30초를 넘길 때까지 기다린다
  static Future<void> _waitForScanSlot() async {
    const window = Duration(seconds: 30);
    final now = DateTime.now();
    _starts.removeWhere((t) => now.difference(t) > window);
    if (_starts.length >= 4) {
      await Future<void>.delayed(window - now.difference(_starts.first) + const Duration(milliseconds: 300));
      _starts.removeAt(0);
    }
  }
}

class BleLink implements BleCommandTransport {
  BleLink._(this.device);

  final BluetoothDevice device;
  late final BluetoothCharacteristic _ssid, _password, _status, _scan, _mac, _firmware, _cmdRx, _cmdTx, _provDone;
  late final BleCommandChannel commands;

  /// 기기 식별자: 0xFF61 STA MAC (12자 대문자 16진수). iOS remoteId는 MAC이 아니라 쓰지 않는다
  late final String mac;
  late final String firmware;

  final _statusController = StreamController<WifiProvStatus>.broadcast();
  final _scanController = StreamController<List<int>>.broadcast();
  final _subs = <StreamSubscription<Object?>>[];
  final _disconnected = Completer<void>();
  bool _bondRetried = false;

  /// 0xFF53 상태 알림
  Stream<WifiProvStatus> get wifiStatus => _statusController.stream;

  /// 연결이 끊기면 완료된다
  Future<void> get disconnected => _disconnected.future;

  static Future<BleLink> connect(BluetoothDevice device, {Duration timeout = const Duration(seconds: 15)}) async {
    final link = BleLink._(device);
    try {
      await device.connect(autoConnect: false, timeout: timeout);
      await link._setup();
      return link;
    } on FlutterBluePlusException catch (e) {
      await link.disconnect();
      throw BleException(BleErrorCode.connectFailed, e.description);
    } catch (_) {
      await link.disconnect();
      rethrow;
    }
  }

  Future<void> _setup() async {
    _subs.add(device.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) _onDisconnected();
    }));
    if (Platform.isAndroid) {
      await device.requestMtu(BleProtocol.androidMtuRequest); // iOS는 OS가 정한 값
    }
    final services = await device.discoverServices();
    BluetoothCharacteristic find(Guid svc, Guid chr) {
      for (final s in services) {
        if (s.uuid != svc) continue;
        for (final c in s.characteristics) {
          if (c.uuid == chr) return c;
        }
      }
      throw BleException(BleErrorCode.serviceMissing, '$svc/$chr');
    }

    _ssid = find(BleProtocol.svcWifi, BleProtocol.chrSsid);
    _password = find(BleProtocol.svcWifi, BleProtocol.chrPassword);
    _status = find(BleProtocol.svcWifi, BleProtocol.chrStatus);
    _scan = find(BleProtocol.svcWifi, BleProtocol.chrScan);
    _mac = find(BleProtocol.svcDevice, BleProtocol.chrMac);
    _firmware = find(BleProtocol.svcDevice, BleProtocol.chrFirmware);
    _cmdRx = find(BleProtocol.svcConfig, BleProtocol.chrCmdRx);
    _cmdTx = find(BleProtocol.svcConfig, BleProtocol.chrCmdTx);
    _provDone = find(BleProtocol.svcConfig, BleProtocol.chrProvDone);

    // 쓰기 전에 알림부터 켠다 (led-app은 나중에 켜서 초기 상태를 놓쳤다)
    _subs.add(_status.onValueReceived.listen((v) {
      final s = v.isEmpty ? null : WifiProvStatus.fromCode(v[0]);
      if (s != null) _statusController.add(s);
    }));
    _subs.add(_scan.onValueReceived.listen(_scanController.add));
    commands = BleCommandChannel(this);
    await _status.setNotifyValue(true);
    await _scan.setNotifyValue(true);
    await _cmdTx.setNotifyValue(true);

    mac = utf8.decode(await _mac.read()).trim().toUpperCase();
    firmware = utf8.decode(await _firmware.read()).trim();
  }

  // ── BleCommandTransport ──

  @override
  int get payloadSize => device.mtuNow - BleProtocol.mtuOverhead;

  @override
  Stream<List<int>> get responses => _cmdTx.onValueReceived;

  @override
  Future<void> writeFrame(List<int> frame) => _write(_cmdRx, frame);

  // ── Wi-Fi ──

  /// 기기가 주변 Wi-Fi를 스캔한다 (기기 스캔이라 iOS도 된다)
  Future<List<WifiNetwork>> scanWifi({Duration timeout = const Duration(seconds: 15)}) async {
    final collector = WifiScanCollector();
    final done = Completer<List<WifiNetwork>>();
    final sub = _scanController.stream.listen((v) {
      try {
        final list = collector.add(v);
        if (list != null && !done.isCompleted) done.complete(list);
      } catch (e) {
        if (!done.isCompleted) done.completeError(e);
      }
    });
    try {
      await _write(_scan, const [1]);
      return await done.future.timeout(timeout,
          onTimeout: () => throw const BleException(BleErrorCode.timeout, 'wifi scan'));
    } finally {
      await sub.cancel();
    }
  }

  /// SSID → 비밀번호를 쓰고 첫 종료 상태(2–5)를 결과로 준다. 기기 제한 20초 + 여유
  Future<WifiProvStatus> provisionWifi(String ssid, String password,
      {Duration timeout = const Duration(seconds: 30)}) async {
    final ssidBytes = utf8.encode(ssid); // led-app은 UTF-16 codeUnits라 한글 SSID가 깨졌다
    final pwBytes = utf8.encode(password);
    if (ssidBytes.isEmpty || ssidBytes.length > BleProtocol.ssidMaxBytes || pwBytes.length > BleProtocol.passwordMaxBytes) {
      throw const BleException(BleErrorCode.tooLarge, 'ssid/password length');
    }
    final result = wifiStatus.firstWhere((s) => s.isFinal).timeout(timeout,
        onTimeout: () => throw const BleException(BleErrorCode.timeout, 'wifi status'));
    await _write(_ssid, ssidBytes);
    await _write(_password, pwBytes); // 쓰면 연결 시도 시작. 개방형은 길이 0
    return result;
  }

  /// 0xFF88: Wi-Fi를 바꿨으면 저장 후 재부팅, 아니면 prov.close와 같다
  Future<void> provisioningDone() => _write(_provDone, const [BleProtocol.provDoneValue]);

  // ── 공통 ──

  Future<void> _write(BluetoothCharacteristic c, List<int> value) async {
    try {
      await c.write(value, withoutResponse: false);
    } on FlutterBluePlusException catch (e) {
      if (!_isAuthError(e)) {
        throw BleException(BleErrorCode.protocol, e.description);
      }
      // 예전 본딩이 남아 암호화 쓰기 실패 (설계서 3.4 "페어링 실패 처리")
      if (Platform.isAndroid && !_bondRetried) {
        _bondRetried = true;
        await device.removeBond();
        return _write(c, value);
      }
      throw BleException(BleErrorCode.bondFailed, e.description);
    }
  }

  /// ATT 0x05 insufficient authentication, 0x0F insufficient encryption
  static bool _isAuthError(FlutterBluePlusException e) => e.code == 5 || e.code == 15;

  void _onDisconnected() {
    if (_disconnected.isCompleted) return;
    _disconnected.complete();
    unawaited(commands.close().catchError((_) {}));
  }

  Future<void> disconnect() async {
    for (final s in _subs) {
      await s.cancel();
    }
    _subs.clear();
    try {
      await device.disconnect();
    } catch (_) {}
    if (!_disconnected.isCompleted) _disconnected.complete();
    await _statusController.close();
    await _scanController.close();
  }
}
