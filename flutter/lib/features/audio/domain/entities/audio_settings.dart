import 'package:equatable/equatable.dart';

/// `/settings/audio` (펌웨어 `device_settings.c` audio_get/audio_set).
/// `spotify_pause_on_voice`는 이 빌드에서 Spotify가 꺼져 있어 다루지 않는다
class AudioSettings extends Equatable {
  const AudioSettings({required this.volume, required this.micGain, required this.autoGain, required this.duckDb});

  /// 0–[maxVolume]
  final int volume;

  /// 하드웨어 마이크 게인 dB ([micGainSteps] 중 하나)
  final int micGain;
  final bool autoGain;

  /// 대화할 때 음악을 줄이는 양 0–[maxDuckDb]
  final int duckDb;

  static const maxVolume = 80; // APP_VOLUME_MAX
  static const maxDuckDb = 40;

  /// 마이크 게인으로 고를 수 있는 값. 코덱은 3 dB 단위이고 33 dB가 없다 (콘솔 `mic`, mic_control.c)
  static const micGainSteps = [0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 36];

  AudioSettings copyWith({int? volume, int? micGain, bool? autoGain, int? duckDb}) => AudioSettings(
        volume: volume ?? this.volume,
        micGain: micGain ?? this.micGain,
        autoGain: autoGain ?? this.autoGain,
        duckDb: duckDb ?? this.duckDb,
      );

  @override
  List<Object?> get props => [volume, micGain, autoGain, duckDb];
}

/// 호출어 (펌웨어 값: 0 Alexa — 2026-10-04 전에는 Jarvis, 1 Computer)
enum WakePhrase {
  alexa(0),
  computer(1);

  const WakePhrase(this.value);
  final int value;

  static WakePhrase fromValue(int? v) => v == 0 ? alexa : computer;
}

/// `/settings/wake` (펌웨어 `device_settings.c` wake_get/wake_set). 기본 Computer·감도 80·+12 dB
class WakeSettings extends Equatable {
  const WakeSettings({required this.enabled, required this.phrase, required this.sensitivity, required this.gainDb});

  final bool enabled;
  final WakePhrase phrase;

  /// 0–100. 높을수록 잘 알아듣지만 오인식이 는다
  final int sensitivity;

  /// 호출어로 가는 소리만 키우는 디지털 게인 0–[maxGainDb] (3 dB 단위로 적용된다, wake.c)
  final int gainDb;

  static const maxGainDb = 24;

  WakeSettings copyWith({bool? enabled, WakePhrase? phrase, int? sensitivity, int? gainDb}) => WakeSettings(
        enabled: enabled ?? this.enabled,
        phrase: phrase ?? this.phrase,
        sensitivity: sensitivity ?? this.sensitivity,
        gainDb: gainDb ?? this.gainDb,
      );

  @override
  List<Object?> get props => [enabled, phrase, sensitivity, gainDb];
}
