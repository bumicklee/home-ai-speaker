# home-ai-speaker

『클로드 코드로 만드는 우리 집 AI 스피커』(이범익 지음, 유페이퍼)의 완성 소스입니다.

Waveshare ESP32-S3-AUDIO-Board로 만든 가족용 AI 스피커 펌웨어와, 스피커를 설정하는 Android 앱 **AI-PA**가 들어 있습니다. 책의 21장까지 마친 **완성본 하나**만 있고, 장별 중간 단계는 따로 두지 않았습니다.

- "Computer"(또는 "Alexa")라고 부르면 한국어로 대답하는 음성 비서
- OpenAI 또는 Google Gemini 중 하나 이상의 API 키로 동작 (실시간 모드 / 절약 모드)
- 인터넷 라디오, 가족 텔레그램 메시지 읽어 주기, 메모·일정 알림, 영어 회화 연습
- 별도 서버 없이 휴대폰과 스피커만으로 동작 (첫 설정은 BLE, 평소 조작은 같은 Wi-Fi의 LAN)

## 폴더

| 폴더 | 내용 |
|:--|:--|
| `firmware/` | 스피커 펌웨어 (ESP-IDF v5.5, C) |
| `flutter/` | 설정 앱 AI-PA (Flutter, Android) |
| `scripts/` | 앱 실행(`run-flutter.sh`)·릴리즈 빌드(`build-flutter-release.sh`) 스크립트 |
| `.claude/` | 이 책의 Claude Code 하네스: `CLAUDE.md`, 스킬 3개(`esp32-firmware`, `flutter-app`, `speaker-method`), 장별 검증 에이전트 |
| `.claude-plugin/` | 하네스를 Claude Code 플러그인으로 설치하기 위한 설정 |
| `certificates/` | OTA 서명 키를 두는 자리. **키는 들어 있지 않습니다** |
| `docs/` | 만들 때 쓴 기능표·설계서와 앱 화면 그림. 독자는 2장에서 자기 문서를 직접 만들고 이것과 비교합니다 ([docs/README.md](docs/README.md)) |

## 이 저장소에 없는 것

API 키, 텔레그램 봇 토큰, Wi-Fi 비밀번호, OTA 서명 키는 들어 있지 않습니다. 키는 책 0장(계정과 키)과 1장(서명 키)을 보고 직접 만듭니다. 비밀값은 앱에서 BLE로만 스피커에 보냅니다.

## 받기

책을 따라 할 때는 1장에서 만드는 내 저장소(`~/home-ai-speaker`)에서 직접 만들고, 이 완성본은 겹치지 않게 다른 폴더에 받아 비교용으로 씁니다.

```bash
git clone https://github.com/bumicklee/home-ai-speaker.git ~/home-ai-speaker-final
```

## 펌웨어 빌드

필요: ESP-IDF v5.5 (`~/esp/esp-idf-v5.5`), Waveshare ESP32-S3-AUDIO-Board

```bash
source ~/esp/esp-idf-v5.5/export.sh

# 1) 서명 키 만들기 (처음 한 번. 키가 없으면 빌드가 실패한다. 책 1.4.9절)
mkdir -p certificates && chmod 700 certificates
espsecure.py generate_signing_key --version 2 --scheme rsa3072 certificates/ota_signing_key.pem
chmod 600 certificates/ota_signing_key.pem

# 2) 빌드와 굽기
cd firmware
idf.py build
idf.py -p <포트> flash monitor
```

서명 키를 잃으면 이미 설치한 스피커를 무선으로 업데이트할 수 없습니다(USB로는 가능). 암호 관리자에 백업해 두세요.

## 앱 실행

필요: Flutter 3.38.5, USB 디버깅을 켠 Android 휴대폰 (스피커와 같은 Wi-Fi)

```bash
cd flutter
flutter pub get
flutter analyze && flutter test
../scripts/run-flutter.sh android
```

릴리즈 APK는 `./scripts/build-flutter-release.sh android`로 만들고 `releases/`에 생깁니다. 이 책의 앱은 스토어에 올리지 않고 `adb install` 또는 파일 복사로 설치합니다. iOS 설정은 코드에 남아 있지만 책에서는 다루지 않습니다.

## Claude Code 하네스

이 저장소에서 Claude Code를 켜면 `.claude/`의 스킬과 에이전트가 자동으로 등록됩니다. 플러그인으로 설치하려면:

```bash
claude plugin marketplace add bumicklee/home-ai-speaker
claude plugin install home-ai-speaker@home-ai-speaker-book
```

자세한 방법은 책 부록 K.9절에 있습니다.

## 버전

검증한 도구·모델·패키지 버전은 [versions.md](versions.md)에 있습니다. 모델 이름이나 라이브러리 버전이 바뀌었을 때 대처법은 책 부록 D를 봅니다.

## 라이선스

- 이 저장소의 코드: [MIT](LICENSE)
- 바탕으로 한 프로젝트와 소리 파일의 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
