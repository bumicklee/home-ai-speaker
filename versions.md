# versions.md — 이 저장소가 검증한 버전

버전이 바뀌는 커밋에는 같은 커밋에서 이 파일을 고친다. 표의 값은 실제로 빌드·검증한 값이다. 자세한 관습은 책 부록 D.5절.

## 도구

| 도구 | 버전 | 확인 명령 |
|:--|:--|:--|
| ESP-IDF | v5.5 (5.5.0) | `idf.py --version` |
| Python (ESP-IDF venv) | 3.14 | `export.sh` 출력 |
| CMake / Ninja | 4.1.2 / 1.13.1 | `cmake --version` |
| Flutter / Dart | 3.38.5 / 3.10.4 | `flutter --version` |

## 펌웨어

| 항목 | 값 |
|:--|:--|
| APP_FW_VERSION | 0.1.0 |
| protocol_version | 1 |
| 실시간 모델 (APP_LIVE_MODEL_DEFAULT) | gpt-live-1 |
| 텍스트 모델 (APP_BACKEND_MODEL_DEFAULT) | gpt-5.6-luna |
| 받아쓰기 (SAVER_STT_MODEL) | gpt-transcribe |
| 음성 합성 (SAVER_TTS_MODEL) | gpt-4o-mini-tts |
| Gemini 절약 (APP_GEMINI_MODEL_DEFAULT) | gemini-3.1-flash-lite |
| Gemini 음성 합성 (APP_GEMINI_TTS_MODEL_DEFAULT) | gemini-3.8-flash-lite-tts |
| Gemini 실시간 (APP_GEMINI_LIVE_MODEL_DEFAULT) | gemini-3.8-live |
| 관리 컴포넌트 | `firmware/dependencies.lock` 참조 (esp-sr 2.1.5, esp_codec_dev 1.5.1, …) |

## 앱

| 항목 | 값 |
|:--|:--|
| version | 1.0.0+1 |
| 패키지 | `flutter/pubspec.lock` 참조 (flutter_bloc 8.1.6, flutter_blue_plus 1.36.8, …) |
| iOS 배포 대상 | 13.0 (코드에만 있음, 책 범위 밖) |

## 변경 기록

- 처음 작성: 책 완성본. ESP-IDF 5.5.0, Flutter 3.38.5. 18장까지 실기기 통과(OpenAI·Gemini × 실시간·절약 네 조합).
