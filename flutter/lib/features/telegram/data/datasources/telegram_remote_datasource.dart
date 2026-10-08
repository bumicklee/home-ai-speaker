import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';

/// 선택된 스피커의 텔레그램 LAN API (설계서 2.14절). 예외는 DeviceApiClient가 AppException으로 바꿔 던진다
class TelegramRemoteDataSource {
  TelegramRemoteDataSource(this._device);

  final CurrentDevice _device;

  /// `{chats, state, bot, unread}`
  Future<Map<String, dynamic>> chats() => _device.client.get<Map<String, dynamic>>(ApiPaths.telegramChats);

  /// `/status`의 `secrets` (키별 bool). state가 off일 때 토큰 유무를 다시 확인한다
  Future<Map<String, dynamic>> secrets() async {
    final j = await _device.client.get<Map<String, dynamic>>(ApiPaths.status);
    return j['secrets'] as Map<String, dynamic>? ?? const {};
  }

  /// 400 INVALID_ARG(빈 값·32바이트 초과), 404 NOT_FOUND
  Future<void> rename(int chatId, String alias) =>
      _device.client.put<dynamic>(ApiPaths.telegramChat(chatId), {'alias': alias});

  Future<void> remove(int chatId) => _device.client.delete<dynamic>(ApiPaths.telegramChat(chatId));

  /// `{auto_read, quiet_start, quiet_end}`
  Future<Map<String, dynamic>> options() => _device.client.get<Map<String, dynamic>>(ApiPaths.telegramOptions);

  /// 저장 뒤 전체 값을 돌려준다. 400 INVALID_ARG(모르는 키·0–23 밖)
  Future<Map<String, dynamic>> saveOptions(Map<String, dynamic> body) =>
      _device.client.put<Map<String, dynamic>>(ApiPaths.telegramOptions, body);

  /// `{code, expires_s, bot}`. 토큰이 없으면 409 NO_TOKEN
  Future<Map<String, dynamic>> pair() => _device.client.post<Map<String, dynamic>>(ApiPaths.telegramPair);
}
