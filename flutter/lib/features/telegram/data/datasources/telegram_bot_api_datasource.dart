import 'package:dio/dio.dart';

import '../../../../core/errors/exceptions.dart';

/// Telegram Bot API (외부). 토큰 확인용 `getMe`만 부른다 (설계서 3.2절).
/// 토큰이 URL에 들어가므로 URL·예외 원문을 로그나 Failure에 남기지 않는다.
class TelegramBotApiDataSource {
  TelegramBotApiDataSource(this._dio);

  final Dio _dio;

  /// BotFather 토큰 모양 (`123456789:AA…`). 맞지 않으면 보내지 않는다 (URL 경로에 그대로 들어간다)
  static final tokenPattern = RegExp(r'^\d{5,}:[A-Za-z0-9_-]{30,}$');

  /// 맞으면 봇 사용자 이름(@ 없이), Telegram이 거절하면(401·404) null
  Future<String?> getMe(String token) async {
    if (!tokenPattern.hasMatch(token)) return null;
    try {
      final r = await _dio.get<Map<String, dynamic>>('https://api.telegram.org/bot$token/getMe');
      final result = r.data?['result'];
      final username = result is Map<String, dynamic> ? result['username'] as String? : null;
      if (r.data?['ok'] != true || username == null) return null;
      return username;
    } on DioException catch (e) {
      final status = e.response?.statusCode;
      if (status == 401 || status == 404) return null; // 형식이 틀린 토큰에는 404를 준다
      // 원문(e.message)에 URL이 들어갈 수 있어 버린다
      throw NetworkException(status == null ? 'telegram unreachable' : 'telegram HTTP $status');
    }
  }
}
