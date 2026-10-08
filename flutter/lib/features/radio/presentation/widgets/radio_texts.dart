import '../../../../core/errors/error_localizer.dart';
import '../../../../core/errors/failures.dart';
import '../../../../l10n/app_localizations.dart';
import '../../domain/radio_rules.dart';

/// 라디오 화면 공통 문구
extension RadioTexts on AppLocalizations {
  String presetErrorText(PresetError e) => switch (e) {
        PresetError.tooMany => radioErrTooMany(RadioLimits.maxPresets),
        PresetError.nameEmpty => radioErrNameEmpty,
        PresetError.nameTooLong => radioErrNameTooLong,
        PresetError.urlInvalid => radioErrUrlInvalid,
        PresetError.urlTooLong => radioErrUrlTooLong,
        PresetError.genreTooLong => radioErrGenreTooLong,
        PresetError.tooLarge => radioErrTooLarge,
        PresetError.duplicate => radioErrDuplicate,
      };

  String presetGroupText(PresetGroup g) => switch (g) {
        PresetGroup.korean => radioGroupKorean,
        PresetGroup.news => radioGroupNews,
        PresetGroup.talk => radioGroupTalk,
        PresetGroup.other => radioGroupOther,
      };

  /// 409 NOT_PLAYING은 공통 키가 없어 여기서 고른다 (공통은 "바빠요"로 보인다)
  String radioFailureText(Failure f) =>
      f is DeviceFailure && f.code == 'NOT_PLAYING' ? radioNotPlaying : errorText(f);
}
