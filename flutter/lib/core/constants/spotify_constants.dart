/// Spotify PKCE 로그인 (설계서 2.10, 3.5절).
/// client_id는 상수가 아니다. 사용자가 자기 개발자 앱을 만들어 입력한다.
library;

class SpotifyConstants {
  SpotifyConstants._();

  /// 리다이렉트 스킴. Android `appAuthRedirectScheme`, iOS `CFBundleURLSchemes`와 같아야 한다 (Q8)
  static const redirectScheme = 'com.aispeaker.app';
  static const redirectUri = '$redirectScheme://spotify-callback';

  static const authorizationEndpoint = 'https://accounts.spotify.com/authorize';
  static const tokenEndpoint = 'https://accounts.spotify.com/api/token';
  static const dashboardUrl = 'https://developer.spotify.com/dashboard';

  /// 설계서 2.10절 (Connect 수신기용 `streaming`은 Q2 결정 뒤에)
  static const scopes = [
    'user-read-playback-state',
    'user-modify-playback-state',
    'user-read-currently-playing',
  ];

  /// client_id 형식: 32자리 16진수
  static final clientIdPattern = RegExp(r'^[0-9a-fA-F]{32}$');
}
