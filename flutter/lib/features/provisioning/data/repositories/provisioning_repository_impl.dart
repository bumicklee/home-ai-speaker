import 'package:dartz/dartz.dart';

import '../../../../core/ble/ble_link.dart';
import '../../../../core/ble/ble_secret_writer.dart';
import '../../../../core/ble/wifi_scan.dart';
import '../../../../core/constants/ble_protocol.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../../../core/services/device_discovery_service.dart';
import '../../../../core/storage/install_state.dart';
import '../../../devices/domain/entities/device.dart';
import '../../../devices/domain/repositories/devices_repository.dart';
import '../../domain/repositories/provisioning_repository.dart';
import '../../../../core/ble/ble_speaker.dart';

class ProvisioningRepositoryImpl implements ProvisioningRepository {
  ProvisioningRepositoryImpl(this._ble, this._discovery, this._devices, this._install);

  final BleSpeakerDataSource _ble;
  final DeviceDiscoveryService _discovery;
  final DevicesRepository _devices;
  final InstallState _install;
  BleSpeakerSession? _session;

  BleSpeakerSession get _s => _session ?? (throw const BleException(BleErrorCode.disconnected));

  @override
  Future<Either<Failure, Unit>> ensureReady() => guard(() async {
        await _ble.ensureReady();
        return unit;
      });

  @override
  Stream<List<DiscoveredSpeaker>> scan() => _ble.scan();

  @override
  Future<String> defaultPhoneName() => _ble.defaultPhoneName();

  @override
  String get phoneId => _install.phoneId;

  @override
  Future<Either<Failure, DeviceInfo>> connect(DiscoveredSpeaker speaker) => guard(() async {
        await _session?.disconnect();
        _session = await _ble.connect(speaker);
        final info = await _s.deviceInfo();
        if (info.needsAppUpdate) {
          throw const BleCommandException(BleCmdError.unsupportedVersion);
        }
        return info;
      });

  @override
  Future<Either<Failure, List<WifiNetwork>>> scanWifi() => guard(() => _s.scanWifi());

  @override
  Future<Either<Failure, WifiProvStatus>> provisionWifi(String ssid, String password) =>
      guard(() => _s.provisionWifi(ssid, password));

  @override
  Future<Either<Failure, Unit>> setSecret(String key, String value) => guard(() async {
        await _s.setSecret(key, value);
        return unit;
      });

  @override
  Future<Either<Failure, PairResult>> pair(String phoneName, {String? evict}) =>
      guard(() => _s.pair(phoneId: phoneId, phoneName: phoneName, evict: evict));

  @override
  Future<Either<Failure, Unit>> setSettings(String group, Map<String, Object?> value) => guard(() async {
        await _s.setSettings(group, value);
        return unit;
      });

  @override
  Future<Either<Failure, Unit>> finish() => guard(() async {
        await _s.finish();
        return unit;
      });

  @override
  Future<void> disconnect() async {
    final s = _session;
    _session = null;
    try {
      await s?.disconnect();
    } catch (_) {}
  }

  @override
  Future<Device> discover(String mac, String name, {Duration timeout = const Duration(seconds: 45)}) async {
    final deadline = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(deadline)) {
      final found = await _discovery.find(mac, timeout: const Duration(seconds: 5));
      if (found != null) return Device(id: mac, name: name, host: found.host, port: found.port);
    }
    return Device(id: mac, name: name);
  }

  @override
  Future<Either<Failure, Unit>> save(Device device, String token) => _devices.addOrUpdate(device, token);
}
