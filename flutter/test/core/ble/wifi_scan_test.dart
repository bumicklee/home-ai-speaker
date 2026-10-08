import 'dart:convert';

import 'package:aispeaker/core/ble/wifi_scan.dart';
import 'package:aispeaker/core/constants/ble_protocol.dart';
import 'package:aispeaker/core/errors/exceptions.dart';
import 'package:flutter_test/flutter_test.dart';

List<int> entry(String ssid, int rssi, int security) {
  final b = List<int>.filled(34, 0);
  final s = utf8.encode(ssid);
  b.setRange(0, s.length, s);
  b[32] = rssi + 128;
  b[33] = security;
  return b;
}

void main() {
  test('한글 SSID(UTF-8), RSSI, 보안 방식', () {
    final n = WifiNetwork.parse(entry('우리집 와이파이', -48, 6));
    expect(n.ssid, '우리집 와이파이');
    expect(n.rssi, -48);
    expect(n.security, WifiSecurity.wpa2Wpa3);
    expect(n.bars, 4);
  });

  test('32바이트를 꽉 채운 SSID', () {
    expect(WifiNetwork.parse(entry('A' * 32, -70, 3)).ssid, 'A' * 32);
  });

  test('모르는 보안 값은 other', () {
    expect(WifiNetwork.parse(entry('x', -90, 42)).security, WifiSecurity.other);
  });

  test('개수 → 항목들, 신호 센 순으로 정렬', () {
    final c = WifiScanCollector();
    expect(c.add([2]), isNull);
    expect(c.add(entry('weak', -80, 3)), isNull);
    final list = c.add(entry('strong', -40, 0))!;
    expect(list.map((n) => n.ssid), ['strong', 'weak']);
    expect(list.first.security.needsPassword, isFalse);
  });

  test('0개면 바로 빈 목록', () {
    expect(WifiScanCollector().add([0]), isEmpty);
  });

  test('0xFF = 기기 스캔 실패', () {
    expect(() => WifiScanCollector().add([BleProtocol.scanFailed]), throwsA(isA<BleException>()));
  });
}
