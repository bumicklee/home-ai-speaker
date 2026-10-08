import 'package:flutter_secure_storage/flutter_secure_storage.dart';

/// 휴대폰에 기억해 두는 비밀값 (2026-10-03 사용자 요청: 한 번 넣은 OpenAI·Gemini 키·텔레그램 봇 토큰을 기억).
/// iOS Keychain / Android Keystore로 암호화된 보안 저장소. 앱을 지웠다 깔면 InstallState가 비운다.
/// 스피커로는 BLE로만 보낸다 (LAN으로 보내지 않음, D6).
class KeyVault {
  KeyVault(this._storage);

  final FlutterSecureStorage _storage;
  static const _openAi = 'vault_openai_key';
  static const _gemini = 'vault_gemini_key';
  static const _telegram = 'vault_tg_bot_token';

  Future<String?> openAiKey() => _storage.read(key: _openAi);
  Future<String?> geminiKey() => _storage.read(key: _gemini);
  Future<String?> telegramToken() => _storage.read(key: _telegram);

  Future<void> saveOpenAiKey(String? value) => _save(_openAi, value);
  Future<void> saveGeminiKey(String? value) => _save(_gemini, value);
  Future<void> saveTelegramToken(String? value) => _save(_telegram, value);

  Future<void> _save(String key, String? value) =>
      value == null || value.isEmpty ? _storage.delete(key: key) : _storage.write(key: key, value: value);
}
