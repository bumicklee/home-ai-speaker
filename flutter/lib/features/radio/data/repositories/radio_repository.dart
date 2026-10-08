import 'package:dartz/dartz.dart';

import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';
import '../../../../core/errors/failures.dart';
import '../../../../core/repository/guard.dart';
import '../../../devices/data/models/device_status_model.dart';
import '../../domain/entities/radio_preset.dart';
import '../../domain/radio_rules.dart';
import '../datasources/radio_browser_datasource.dart';

/// 라디오: 프리셋·재생은 선택된 스피커로 (local_api.c), 방송 찾기는 Radio-Browser로
class RadioRepository {
  RadioRepository(this._device, this._browser);

  final CurrentDevice _device;
  final RadioBrowserDataSource _browser;

  /// GET /radio/presets → `{"presets": [...]}`
  Future<Either<Failure, List<RadioPreset>>> presets() =>
      guard(() async => parsePresets(await _device.client.get<dynamic>(ApiPaths.radioPresets)));

  /// PUT /radio/presets `{"presets": [...]}` (전체 교체). 기기가 저장한 목록을 돌려준다. 검증 실패 400 INVALID_ARG
  Future<Either<Failure, List<RadioPreset>>> savePresets(List<RadioPreset> presets) =>
      guard(() async => parsePresets(await _device.client.put<dynamic>(ApiPaths.radioPresets, presetsBody(presets))));

  /// GET /media/now (라디오가 꺼져 있고 Spotify가 연결돼 있으면 Spotify 상태)
  Future<Either<Failure, RadioNow>> now() =>
      guard(() async => parseNow(await _device.client.get<dynamic>(ApiPaths.mediaNow) as Map<String, dynamic>));

  /// POST /media/play `{"preset": n}`. 없는 번호면 400
  Future<Either<Failure, Unit>> playPreset(int index) => _post(ApiPaths.mediaPlay, {'preset': index});

  /// POST /media/play `{"url", "title"}`. http(s)가 아니거나 256바이트를 넘으면 400
  Future<Either<Failure, Unit>> playUrl(String url, String title) =>
      _post(ApiPaths.mediaPlay, {'url': url, 'title': title});

  /// pause / resume / stop / next / previous. 재생 중이 아니면 409 NOT_PLAYING
  Future<Either<Failure, Unit>> media(String action) => _post(ApiPaths.mediaControl(action));

  Future<Either<Failure, List<RadioStation>>> search(String query, {bool byTag = false}) =>
      guard(() => _browser.search(query, byTag: byTag));

  Future<Either<Failure, Unit>> _post(String path, [Object? body]) => guard(() async {
        await _device.client.post<dynamic>(path, body);
        return unit;
      });

  static List<RadioPreset> parsePresets(dynamic body) {
    final list = body is Map ? body['presets'] : body;
    return [for (final p in (list as List? ?? const []).whereType<Map<String, dynamic>>()) RadioPreset.fromJson(p)];
  }

  /// `/media/now`, WS `media_state` 공통 (event 키는 무시)
  static RadioNow parseNow(Map<String, dynamic> j) =>
      RadioNow(media: DeviceStatusModel.mediaFromJson(j), preset: (j['preset'] as num?)?.toInt());
}
