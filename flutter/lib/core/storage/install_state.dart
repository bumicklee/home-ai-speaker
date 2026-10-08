import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:uuid/uuid.dart';

/// 설치 단위 상태 (설계서 3.6 "재설치 처리").
/// iOS Keychain은 앱을 지워도 남으므로, 첫 실행 표시가 없으면 secure storage를 비운다.
class InstallState {
  InstallState(this._prefs, this._secure);

  final SharedPreferences _prefs;
  final FlutterSecureStorage _secure;

  static const _installedKey = 'installed';
  static const _phoneIdKey = 'phone_id';

  /// 앱 시작 시 한 번
  Future<void> init() async {
    if (_prefs.getBool(_installedKey) != true) {
      await _secure.deleteAll();
      await _prefs.setBool(_installedKey, true);
    }
    if (_prefs.getString(_phoneIdKey) == null) {
      await _prefs.setString(_phoneIdKey, const Uuid().v4());
    }
  }

  /// `api.pair`의 phone_id. 설치마다 새로 만든다 (재설치하면 새 휴대폰으로 등록)
  String get phoneId => _prefs.getString(_phoneIdKey)!;
}
