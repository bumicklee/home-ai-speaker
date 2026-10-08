/// 휴대폰 역지오코딩 결과 → 스피커에 저장할 동네 이름 (설계서 3.0절).
/// 한국은 `시/도 구/군 동`만 쓰고 번지·건물명·도로명은 버린다 (예: "서울 강남구 역삼동").
library;

import '../entities/assistant_settings.dart';

/// 역지오코딩 주소 조각 (geocoding `Placemark`에서 필요한 것만. 플러그인 없이 시험할 수 있게)
class PlaceParts {
  const PlaceParts({
    this.isoCountryCode,
    this.country,
    this.administrativeArea,
    this.subAdministrativeArea,
    this.locality,
    this.subLocality,
    this.thoroughfare,
    this.name,
  });

  final String? isoCountryCode;
  final String? country;
  final String? administrativeArea;
  final String? subAdministrativeArea;
  final String? locality;
  final String? subLocality;
  final String? thoroughfare;
  final String? name;
}

class PlaceFormatter {
  PlaceFormatter._();

  static final _sido = RegExp(r'^[가-힣]+(특별시|광역시|특별자치시|특별자치도|도)$');
  static final _sigungu = RegExp(r'^[가-힣]+(시|군|구)$');

  /// 동·읍·면, 숫자 붙은 동(역삼1동), 가(을지로3가)
  static final _dong = RegExp(r'^[가-힣]+(\d*동|읍|면|\d+가)$');

  /// 못 만들면 빈 문자열
  static String format(PlaceParts p) {
    final text = p.isoCountryCode?.toUpperCase() == 'KR' || _looksKorean(p) ? _korean(p) : _other(p);
    return _fit(text);
  }

  static bool _looksKorean(PlaceParts p) => p.isoCountryCode == null && _sido.hasMatch(p.administrativeArea ?? '');

  /// 플랫폼마다 필드 배치가 달라서 (Android: locality=구, iOS: subLocality=구 등) 모든 조각을 낱말로 나눠 고른다
  static String _korean(PlaceParts p) {
    final words = <String>[];
    for (final f in [p.administrativeArea, p.subAdministrativeArea, p.locality, p.subLocality, p.thoroughfare, p.name]) {
      for (final w in (f ?? '').split(RegExp(r'\s+'))) {
        if (w.isNotEmpty && !words.contains(w)) words.add(w);
      }
    }
    String? sido;
    final sigungu = <String>[];
    String? dong;
    for (final w in words) {
      if (sido == null && _sido.hasMatch(w)) {
        sido = w;
      } else if (_sigungu.hasMatch(w) && w != sido && sigungu.length < 2 && !sigungu.contains(w)) {
        sigungu.add(w); // 수원시 영통구처럼 시·구가 둘일 수 있다
      } else if (dong == null && _dong.hasMatch(w)) {
        dong = w;
      }
    }
    return [if (sido != null) _shortSido(sido), ...sigungu, ?dong].join(' ');
  }

  /// 서울특별시 → 서울, 부산광역시 → 부산, 세종특별자치시 → 세종, 제주특별자치도 → 제주도. 경기도는 그대로
  static String _shortSido(String s) {
    for (final suffix in ['특별자치시', '특별시', '광역시']) {
      if (s.endsWith(suffix)) return s.substring(0, s.length - suffix.length);
    }
    if (s.endsWith('특별자치도')) return '${s.substring(0, s.length - 5)}도';
    return s;
  }

  /// 해외: "동네, 도시, 주, 나라" 중 있는 것 (번지·도로명 없이)
  static String _other(PlaceParts p) {
    final parts = <String>[];
    for (final f in [p.subLocality, p.locality, p.administrativeArea, p.country]) {
      final v = f?.trim() ?? '';
      if (v.isNotEmpty && !parts.contains(v)) parts.add(v);
    }
    return parts.join(', ');
  }

  /// 96바이트를 넘으면 앞(큰 지역)부터 남기고 뒤를 버린다
  static String _fit(String text) {
    var t = text;
    while (utf8Bytes(t) > AssistantLimits.locationBytes) {
      final cut = t.lastIndexOf(RegExp(r'[ ,]'));
      t = cut > 0 ? t.substring(0, cut).replaceAll(RegExp(r'[ ,]+$'), '') : '';
    }
    return t;
  }
}

/// 휴대폰 시간대를 스피커가 아는지. 모르면 고를 후보 (Asia/Seoul, UTC — 가까운 쪽 먼저)
class TimezoneSuggestion {
  const TimezoneSuggestion._(this.phone, this.supported, this.fallbacks);

  factory TimezoneSuggestion.of(String? phone) {
    if (phone != null && AssistantLimits.timezones.contains(phone)) {
      return TimezoneSuggestion._(phone, true, const []);
    }
    final asia = phone != null && (phone.startsWith('Asia/') || phone == 'ROK' || phone == 'Japan');
    return TimezoneSuggestion._(phone, false, asia ? const ['Asia/Seoul', 'UTC'] : const ['UTC', 'Asia/Seoul']);
  }

  /// 휴대폰 IANA 이름 (못 읽으면 null)
  final String? phone;

  /// 펌웨어 `TIMEZONES`에 있음 → 그대로 쓸 수 있다
  final bool supported;
  final List<String> fallbacks;
}
