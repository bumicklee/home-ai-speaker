import 'package:dio/dio.dart';

/// 외부 API용 (Radio-Browser, Telegram getMe). 기기 토큰을 붙이지 않는다.
Dio createExternalHttp() => Dio(BaseOptions(
      connectTimeout: const Duration(seconds: 8),
      receiveTimeout: const Duration(seconds: 15),
      // Radio-Browser는 User-Agent를 요구한다
      headers: {'User-Agent': 'AISpeakerApp/1.0'},
    ));
