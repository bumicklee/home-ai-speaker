/// 0xFF54 Wi-Fi 스캔 알림 해석 (설계서 2.13절).
/// 순서: 개수 1바이트(0–20, 0xFF = 실패) → AP마다 34바이트 [SSID 32B(0 채움)][RSSI+128][보안].
library;

import 'dart:convert';

import 'package:equatable/equatable.dart';

import '../constants/ble_protocol.dart';
import '../errors/exceptions.dart';

class WifiNetwork extends Equatable {
  const WifiNetwork({required this.ssid, required this.rssi, required this.security});

  final String ssid;
  final int rssi;
  final WifiSecurity security;

  /// 신호 막대 0–4
  int get bars => rssi >= -55
      ? 4
      : rssi >= -67
          ? 3
          : rssi >= -75
              ? 2
              : rssi >= -85
                  ? 1
                  : 0;

  static WifiNetwork parse(List<int> entry) {
    if (entry.length != BleProtocol.scanEntryBytes) {
      throw BleException(BleErrorCode.protocol, 'scan entry ${entry.length} bytes');
    }
    var end = 0;
    while (end < BleProtocol.ssidMaxBytes && entry[end] != 0) {
      end++;
    }
    return WifiNetwork(
      ssid: utf8.decode(entry.sublist(0, end), allowMalformed: true),
      rssi: entry[32] - 128,
      security: WifiSecurity.fromCode(entry[33]),
    );
  }

  @override
  List<Object?> get props => [ssid, rssi, security];
}

/// 알림을 차례로 넣으면 목록이 다 모였을 때 결과를 준다.
class WifiScanCollector {
  int? _expected;
  final _networks = <WifiNetwork>[];

  /// 다 모이면 신호 센 순으로 정렬한 목록, 아직이면 null
  List<WifiNetwork>? add(List<int> notification) {
    if (_expected == null) {
      if (notification.length != 1) {
        throw const BleException(BleErrorCode.protocol, 'scan count expected');
      }
      final n = notification[0];
      if (n == BleProtocol.scanFailed) {
        throw const BleException(BleErrorCode.protocol, 'device scan failed');
      }
      if (n > BleProtocol.scanMaxEntries) {
        throw BleException(BleErrorCode.protocol, 'scan count $n');
      }
      _expected = n;
    } else {
      _networks.add(WifiNetwork.parse(notification));
    }
    if (_networks.length < _expected!) return null;
    return List.of(_networks)..sort((a, b) => b.rssi.compareTo(a.rssi));
  }
}
