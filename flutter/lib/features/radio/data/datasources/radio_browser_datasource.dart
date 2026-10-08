import 'package:dio/dio.dart';

import '../../../../core/errors/exceptions.dart';
import '../../domain/entities/radio_preset.dart';
import '../../domain/radio_rules.dart';

/// Radio-Browser 방송 검색 (휴대폰에서 바로, 설계서 6장 "라디오"). 외부 Dio를 쓴다 (User-Agent 필수)
class RadioBrowserDataSource {
  RadioBrowserDataSource(this._dio);

  final Dio _dio;

  /// 펌웨어 `radio_search.c`와 같은 서버
  static const host = 'https://de1.api.radio-browser.info';
  static const limit = 30;

  /// 인기 순. 스피커가 틀 수 없는 방송(HLS, 다른 코덱)은 뺀다. 같은 주소는 한 번만
  Future<List<RadioStation>> search(String query, {bool byTag = false}) async {
    try {
      final r = await _dio.get<List<dynamic>>(
        '$host/json/stations/search',
        queryParameters: {
          byTag ? 'tag' : 'name': query,
          'hidebroken': 'true',
          'order': 'clickcount',
          'reverse': 'true',
          'limit': limit,
        },
      );
      final seen = <String>{};
      return [
        for (final s in (r.data ?? const []).whereType<Map<String, dynamic>>())
          if (isPlayable(s) && seen.add(s['url_resolved'] as String))
            RadioStation(
              name: (s['name'] as String? ?? '').trim(),
              url: s['url_resolved'] as String,
              codec: s['codec'] as String,
              bitrate: (s['bitrate'] as num? ?? 0).toInt(),
              country: s['countrycode'] as String? ?? '',
              tags: s['tags'] as String? ?? '',
            ),
      ];
    } on DioException catch (e) {
      throw NetworkException(e.message);
    }
  }
}
