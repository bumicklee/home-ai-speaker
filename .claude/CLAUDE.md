# home-ai-speaker

『클로드 코드로 만드는 우리 집 AI 스피커』를 따라 만드는 저장소다. Waveshare ESP32-S3-AUDIO-Board 펌웨어(`firmware/`, ESP-IDF v5.5)와 설정 앱 AI-PA(`flutter/`, Android)로 이루어진다.

## 빌드

- 펌웨어: `source ~/esp/esp-idf-v5.5/export.sh` → `cd firmware && idf.py build` → `idf.py -p <포트> flash monitor`
- 앱: `cd flutter && flutter pub get && flutter analyze && flutter test` → `../scripts/run-flutter.sh android` (스크립트가 아직 없으면 `flutter run`)

## 규칙

- 펌웨어 작업 규칙은 `esp32-firmware` 스킬, 앱 작업 규칙은 `flutter-app` 스킬, 기획·설계·장애 보고서 양식은 `speaker-method` 스킬에 있다.
- 비밀값(AI 키, 봇 토큰, `certificates/*.pem`)은 코드·로그·Git에 넣지 않는다.
- 한 번에 한 단계만 하고, 확인 명령을 돌려 결과를 보여 준다.
- 커밋은 사용자가 요청할 때만 한다.

## 문서

- `docs/01-plan/`: 기획서·기능표(2장에서 만든다)
- `docs/design.md`: 설계서(2장에서 만든다). 기능별 설계서는 `docs/design-<기능>.md`
- 책의 완성본 저장소에는 만들 때 쓴 문서가 `docs/`에 들어 있다. 비교용으로 읽는다
- `docs/HANDOFF.md`: 다음 세션이 가장 먼저 읽는 인수인계 문서
