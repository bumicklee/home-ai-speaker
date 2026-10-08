import 'package:equatable/equatable.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// 앱 전역 설정: 테마, 언어 (led-app 방식). 기본 언어는 시스템 언어, 지원하지 않으면 ko.
class SettingsState extends Equatable {
  const SettingsState({this.themeMode = ThemeMode.system, this.locale});

  final ThemeMode themeMode;

  /// null = 시스템 언어
  final Locale? locale;

  SettingsState copyWith({ThemeMode? themeMode, Locale? Function()? locale}) =>
      SettingsState(themeMode: themeMode ?? this.themeMode, locale: locale != null ? locale() : this.locale);

  @override
  List<Object?> get props => [themeMode, locale];
}

class SettingsCubit extends Cubit<SettingsState> {
  SettingsCubit(this._prefs) : super(_load(_prefs));

  final SharedPreferences _prefs;
  static const _themeKey = 'theme_mode';
  static const _localeKey = 'locale';
  static const supportedLanguages = ['ko', 'en'];

  static SettingsState _load(SharedPreferences p) {
    final theme = ThemeMode.values.asNameMap()[p.getString(_themeKey)] ?? ThemeMode.system;
    final lang = p.getString(_localeKey);
    return SettingsState(themeMode: theme, locale: supportedLanguages.contains(lang) ? Locale(lang!) : null);
  }

  Future<void> setThemeMode(ThemeMode mode) async {
    await _prefs.setString(_themeKey, mode.name);
    emit(state.copyWith(themeMode: mode));
  }

  /// null = 시스템 언어
  Future<void> setLanguage(String? languageCode) async {
    if (languageCode == null) {
      await _prefs.remove(_localeKey);
      emit(state.copyWith(locale: () => null));
    } else if (supportedLanguages.contains(languageCode)) {
      await _prefs.setString(_localeKey, languageCode);
      emit(state.copyWith(locale: () => Locale(languageCode)));
    }
  }

  /// 시스템 언어가 지원 목록에 없으면 ko
  static Locale resolve(Locale? device, Iterable<Locale> supported) {
    for (final l in supported) {
      if (l.languageCode == device?.languageCode) return l;
    }
    return const Locale('ko');
  }
}
