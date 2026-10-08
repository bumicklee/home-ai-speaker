import 'package:flutter/material.dart';
import 'package:flutter_native_splash/flutter_native_splash.dart';

import 'app.dart';
import 'features/devices/presentation/bloc/devices_cubit.dart';
import 'injection_container.dart';

Future<void> main() async {
  final binding = WidgetsFlutterBinding.ensureInitialized();
  // 네이티브 스플래시(디자인 "스플래시")를 기기 목록을 읽을 때까지 유지한다 → 빈 로딩 화면이 깜빡이지 않게
  FlutterNativeSplash.preserve(widgetsBinding: binding);
  await initDependencies();
  sl<DevicesCubit>().load().whenComplete(FlutterNativeSplash.remove);
  runApp(const AiSpeakerApp());
}
