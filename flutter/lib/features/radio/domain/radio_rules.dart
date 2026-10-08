/// 라디오 규칙. 펌웨어와 같아야 한다:
/// - 프리셋 검증: `radio_presets.c` `presets_valid()` + `media_radio.h` 한도 + `local_api.c` BODY_MAX
/// - 틀 수 있는 방송: `radio_search.c` `playable()`
/// 길이는 펌웨어가 `strlen`으로 재므로 UTF-8 바이트 수다 (한글 한 글자 = 3바이트).
library;

import 'dart:convert';

import 'entities/radio_preset.dart';

class RadioLimits {
  RadioLimits._();

  /// RADIO_MAX_PRESETS
  static const maxPresets = 20;

  /// RADIO_NAME_MAX (바이트)
  static const nameMax = 48;

  /// RADIO_URL_MAX (바이트)
  static const urlMax = 256;

  /// "http://" + 한 글자. 펌웨어는 8바이트 미만 주소를 거부한다
  static const urlMin = 8;

  /// RADIO_GENRE_MAX (바이트)
  static const genreMax = 64;

  /// local_api.c BODY_MAX: PUT 본문 전체
  static const bodyMax = 8192;
}

enum PresetError { tooMany, nameEmpty, nameTooLong, urlInvalid, urlTooLong, genreTooLong, tooLarge, duplicate }

int utf8Length(String s) => utf8.encode(s).length;

bool isHttpUrl(String url) => url.startsWith('http://') || url.startsWith('https://');

/// 프리셋 하나 검증 (펌웨어 순서: 이름 → 주소 → 장르)
PresetError? validatePreset(RadioPreset p) {
  final name = utf8Length(p.name);
  if (name < 1) return PresetError.nameEmpty;
  if (name > RadioLimits.nameMax) return PresetError.nameTooLong;
  final url = utf8Length(p.url);
  if (url > RadioLimits.urlMax) return PresetError.urlTooLong;
  if (url < RadioLimits.urlMin || !isHttpUrl(p.url)) return PresetError.urlInvalid;
  if (utf8Length(p.genre) > RadioLimits.genreMax) return PresetError.genreTooLong;
  return null;
}

/// PUT 전에 목록 전체 검증. 통과하면 null
PresetError? validatePresets(List<RadioPreset> presets) {
  if (presets.length > RadioLimits.maxPresets) return PresetError.tooMany;
  for (final p in presets) {
    final e = validatePreset(p);
    if (e != null) return e;
  }
  if (utf8Length(jsonEncode(presetsBody(presets))) > RadioLimits.bodyMax) return PresetError.tooLarge;
  return null;
}

Map<String, dynamic> presetsBody(List<RadioPreset> presets) => {'presets': [for (final p in presets) p.toJson()]};

/// 바이트 한도에 맞게 자른다 (글자 중간에서 자르지 않는다)
String truncateUtf8(String s, int maxBytes) {
  if (utf8Length(s) <= maxBytes) return s;
  final out = StringBuffer();
  var used = 0;
  for (final r in s.runes) {
    final n = utf8Length(String.fromCharCode(r));
    if (used + n > maxBytes) break;
    out.writeCharCode(r);
    used += n;
  }
  return out.toString().trimRight();
}

/// 입력한 태그 → genre 문자열 (공백·대문자·빈 태그 정리, 길이는 자르지 않는다)
String normalizeGenre(String text) =>
    text.split(',').map((t) => t.trim().toLowerCase()).where((t) => t.isNotEmpty).toSet().join(',');

/// 검색 결과 태그 → genre 문자열. 한도를 넘는 태그는 통째로 뺀다
String genreFromTags(Iterable<String> tags) {
  final out = <String>[];
  for (final t in tags.map((t) => t.trim().toLowerCase()).where((t) => t.isNotEmpty)) {
    if (out.contains(t)) continue;
    if (utf8Length([...out, t].join(',')) > RadioLimits.genreMax) break;
    out.add(t);
  }
  return out.join(',');
}

/// Radio-Browser 방송 하나가 스피커에서 재생될지 (`radio_search.c` `playable()`와 같은 규칙)
/// - `url_resolved`가 http(s)이고 256바이트 이하
/// - HLS가 아님: 주소에 `.m3u8`가 없고 `hls` 표시가 0
/// - 코덱이 MP3 또는 AAC로 시작 (AAC+ 등)
bool isPlayable(Map<String, dynamic> s) {
  final codec = s['codec'];
  final url = s['url_resolved'];
  if (codec is! String || url is! String || utf8Length(url) > RadioLimits.urlMax) return false;
  final hlsFlag = s['hls'];
  final hls = url.contains('.m3u8') || (hlsFlag is num && hlsFlag.toInt() != 0);
  final codecOk = codec == 'MP3' || codec.startsWith('AAC');
  return isHttpUrl(url) && !hls && codecOk;
}

/// 검색 결과 → 프리셋 (이름·장르를 펌웨어 한도에 맞게 자른다)
RadioPreset presetFromStation(RadioStation s) => RadioPreset(
      name: truncateUtf8(s.name.trim().isEmpty ? 'Radio' : s.name.trim(), RadioLimits.nameMax),
      url: s.url,
      genre: genreFromTags(s.tags.split(',')),
    );

/// 화면 묶음 (디자인 순서)
enum PresetGroup { korean, news, talk, other }

const _talkTags = {'talk', 'community', 'politics'};
const _newsTags = {'news', 'business', 'finance', 'world', 'economy'};
const _koreanTags = {'korean', 'kpop', 'k-pop', 'korea', 'trot'};

/// 묶음 정하기. 말 위주가 음악보다, 뉴스·토크가 함께 있으면 앞에 나온 태그가 이긴다
/// (CNN "news,talk" → 뉴스·경제, NPR "talk,news" → 토크. 디자인과 같다)
PresetGroup groupOf(RadioPreset p) {
  final tags = p.tags;
  final talkAt = tags.indexWhere(_talkTags.contains);
  final newsAt = tags.indexWhere(_newsTags.contains);
  if (talkAt >= 0 && (newsAt < 0 || talkAt < newsAt)) return PresetGroup.talk;
  if (newsAt >= 0) return PresetGroup.news;
  if (tags.any(_koreanTags.contains)) return PresetGroup.korean;
  return PresetGroup.other;
}

/// 프리셋 번호를 묶음별로 (모든 프리셋이 정확히 한 번, 묶음 안에서는 저장 순서). 빈 묶음은 뺀다
List<(PresetGroup, List<int>)> groupPresets(List<RadioPreset> presets) {
  final groups = {for (final g in PresetGroup.values) g: <int>[]};
  for (var i = 0; i < presets.length; i++) {
    groups[groupOf(presets[i])]!.add(i);
  }
  return [
    for (final g in PresetGroup.values)
      if (groups[g]!.isNotEmpty) (g, groups[g]!),
  ];
}

/// 지금 틀고 있는 프리셋 번호. 순서를 바꾼 뒤에도 맞게 이름을 먼저 본다
int? playingPresetIndex(List<RadioPreset> presets, RadioNow now) {
  if (!now.isRadio) return null;
  final i = now.preset;
  final station = now.media.station;
  if (i != null && i >= 0 && i < presets.length && (station.isEmpty || presets[i].name == station)) return i;
  if (station.isEmpty) return null;
  final byName = presets.indexWhere((p) => p.name == station);
  return byName >= 0 ? byName : null;
}
