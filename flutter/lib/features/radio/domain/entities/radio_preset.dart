import 'package:equatable/equatable.dart';

import '../../../devices/domain/entities/device_status.dart';

/// 프리셋 하나. 펌웨어 `/storage/presets.json`의 `{name, url, genre}` (genre는 쉼표로 구분한 태그)
class RadioPreset extends Equatable {
  const RadioPreset({required this.name, required this.url, this.genre = ''});

  factory RadioPreset.fromJson(Map<String, dynamic> j) => RadioPreset(
        name: j['name'] as String? ?? '',
        url: j['url'] as String? ?? '',
        genre: j['genre'] as String? ?? '',
      );

  final String name;
  final String url;
  final String genre;

  /// 소문자 태그 (빈 것은 뺀다)
  List<String> get tags =>
      genre.split(',').map((t) => t.trim().toLowerCase()).where((t) => t.isNotEmpty).toList(growable: false);

  /// 화면에 보이는 태그. korean·english는 묶음을 정하는 표시라 뺀다 (디자인 "kpop · pop")
  List<String> get displayTags => tags.where((t) => t != 'korean' && t != 'english').toList(growable: false);

  Map<String, dynamic> toJson() => {'name': name, 'url': url, 'genre': genre};

  RadioPreset copyWith({String? name, String? url, String? genre}) =>
      RadioPreset(name: name ?? this.name, url: url ?? this.url, genre: genre ?? this.genre);

  @override
  List<Object?> get props => [name, url, genre];
}

/// `/media/now`, WS `media_state`. [preset]은 프리셋으로 틀었을 때만 (펌웨어 `radio_now_json`)
class RadioNow extends Equatable {
  const RadioNow({this.media = const MediaNow(), this.preset});

  final MediaNow media;
  final int? preset;

  bool get isRadio => media.source == 'radio' && media.isActive;

  /// 일시정지 버튼을 보일지 (재생·연결 중)
  bool get canPause => media.state == 'playing' || media.state == 'buffering';

  @override
  List<Object?> get props => [media, preset];
}

/// Radio-Browser 검색 결과 중 스피커가 틀 수 있는 방송
class RadioStation extends Equatable {
  const RadioStation({
    required this.name,
    required this.url,
    this.codec = '',
    this.bitrate = 0,
    this.country = '',
    this.tags = '',
  });

  final String name;

  /// `url_resolved` (재생목록을 푼 실제 스트림 주소)
  final String url;
  final String codec;
  final int bitrate;
  final String country;
  final String tags;

  @override
  List<Object?> get props => [name, url, codec, bitrate, country, tags];
}
