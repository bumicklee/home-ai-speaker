/// 같은 Wi-Fi의 스피커 찾기 (mDNS `_aispeaker._tcp`, 설계서 2.14절).
/// iOS는 Info.plist `NSBonjourServices`에 이 서비스가 있어야 한다 (`multicast_dns`는 entitlement가 필요해 `nsd` 사용).
library;

import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:equatable/equatable.dart';
import 'package:nsd/nsd.dart' as nsd;

import '../constants/api_paths.dart';
import '../network/device_endpoint.dart';

class FoundSpeaker extends Equatable {
  const FoundSpeaker({required this.shortId, required this.host, required this.port, this.protocolVersion});

  /// TXT `id` = STA MAC 뒤 6자리 (기기 식별자 12자의 끝)
  final String shortId;
  final String host;
  final int port;
  final int? protocolVersion;

  bool matches(String deviceId) => deviceId.toUpperCase().endsWith(shortId.toUpperCase());

  @override
  List<Object?> get props => [shortId, host, port, protocolVersion];
}

class DeviceDiscoveryService implements DeviceResolver {
  /// [timeout] 동안 찾은 스피커 목록
  Future<List<FoundSpeaker>> discover({Duration timeout = const Duration(seconds: 4)}) async {
    final found = <String, FoundSpeaker>{};
    final discovery = await nsd.startDiscovery(LanDiscovery.serviceType, ipLookupType: nsd.IpLookupType.v4);
    try {
      void collect() {
        for (final s in discovery.services) {
          final speaker = _parse(s);
          if (speaker != null) found[speaker.shortId] = speaker;
        }
      }

      discovery.addListener(collect);
      await Future<void>.delayed(timeout);
      discovery.removeListener(collect);
      collect();
    } finally {
      await nsd.stopDiscovery(discovery);
    }
    return found.values.toList();
  }

  /// 한 기기를 찾으면 바로 돌려준다
  Future<FoundSpeaker?> find(String deviceId, {Duration timeout = const Duration(seconds: 4)}) async {
    final discovery = await nsd.startDiscovery(LanDiscovery.serviceType, ipLookupType: nsd.IpLookupType.v4);
    final result = Completer<FoundSpeaker?>();
    void check() {
      for (final s in discovery.services) {
        final speaker = _parse(s);
        if (speaker != null && speaker.matches(deviceId) && !result.isCompleted) result.complete(speaker);
      }
    }

    discovery.addListener(check);
    check();
    try {
      return await result.future.timeout(timeout, onTimeout: () => null);
    } finally {
      discovery.removeListener(check);
      await nsd.stopDiscovery(discovery);
    }
  }

  @override
  Future<DeviceEndpoint?> resolve(DeviceEndpoint stale) async {
    final speaker = await find(stale.deviceId);
    return speaker == null ? null : stale.copyWith(host: speaker.host, port: speaker.port);
  }

  static FoundSpeaker? _parse(nsd.Service s) {
    final id = _txt(s, LanDiscovery.txtId);
    final ip = s.addresses?.firstWhere((a) => a.type == InternetAddressType.IPv4, orElse: () => s.addresses!.first);
    if (id == null || id.isEmpty || ip == null) return null;
    return FoundSpeaker(
      shortId: id.toUpperCase(),
      host: ip.address,
      port: s.port ?? LanDiscovery.port,
      protocolVersion: int.tryParse(_txt(s, LanDiscovery.txtProtocol) ?? ''),
    );
  }

  static String? _txt(nsd.Service s, String key) {
    final v = s.txt?[key];
    return v == null ? null : utf8.decode(v, allowMalformed: true);
  }
}
