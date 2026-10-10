import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import 'app/app_router.dart';
import 'app/routes.dart';
import 'core/settings/settings_cubit.dart';
import 'core/theme/app_theme.dart';
import 'features/devices/presentation/bloc/devices_cubit.dart';
import 'injection_container.dart';
import 'l10n/app_localizations.dart';

/// 앱 루트. 앱 전역 상태(설정, 기기 선택)만 여기서 주고, 기능 BLoC는 페이지 단위로 만든다
/// (루트에서 전부 만들면 app.dart가 천 줄을 넘기 쉽다).
class AiSpeakerApp extends StatelessWidget {
  const AiSpeakerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiBlocProvider(
      providers: [
        BlocProvider.value(value: sl<SettingsCubit>()),
        BlocProvider.value(value: sl<DevicesCubit>()),
      ],
      child: BlocBuilder<SettingsCubit, SettingsState>(
        builder: (context, settings) => MaterialApp(
          onGenerateTitle: (context) => AppLocalizations.of(context).appTitle,
          theme: AppTheme.light(),
          darkTheme: AppTheme.dark(),
          themeMode: settings.themeMode,
          locale: settings.locale,
          supportedLocales: AppLocalizations.supportedLocales,
          localizationsDelegates: AppLocalizations.localizationsDelegates,
          localeResolutionCallback: (device, supported) => SettingsCubit.resolve(device, supported),
          initialRoute: AppRoutes.home,
          onGenerateRoute: onGenerateRoute,
        ),
      ),
    );
  }
}
