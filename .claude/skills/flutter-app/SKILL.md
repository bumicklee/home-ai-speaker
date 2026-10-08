---
name: flutter-app
description: 스피커 설정 앱 AI-PA(Flutter, Android) 작업 규칙과 책의 단계별 프롬프트. flutter/ 아래 Dart 코드, 화면, BLE·LAN 통신 층, 문구 파일(ARB), 테스트, 릴리즈 APK를 다룰 때 쓴다.
when_to_use: Flutter, Dart, 앱 화면, Cubit, BLoC, 첫 설정, BLE 스캔, mDNS 찾기, LAN API 호출, WebSocket 이벤트, flutter test, APK
paths: flutter/**
argument-hint: "[장 번호, 예: 12]"
---

# 앱 작업 규칙 (책 『클로드 코드로 만드는 우리 집 AI 스피커』)

인자로 장 번호가 오면(`/flutter-app 12`) [prompts.md](prompts.md)에서 그 장의 프롬프트를 찾아 순서대로 진행한다. 한 번에 한 단계만 하고, 단계마다 아래 "확인 순서"를 돌린다.

## 지켜야 할 규칙

- **이 책의 앱은 Android 전용이다.** iOS 빌드·코드 서명은 다루지 않는다. 코드에 있는 iOS 설정은 지우지 않고 그대로 둔다.
- **스토어에 올리지 않는다.** 릴리즈 APK는 디버그 키로 서명한 채 `adb install` 또는 파일 복사로 설치한다.
- **기능 폴더는 `data` · `domain` · `presentation` 세 층**으로 나눈다. 화면은 BLE·LAN을 직접 모른다. 레포지토리를 통해서만 부른다.
- **상태는 Cubit**으로 다루고, 상태 객체는 `copyWith`로 새로 만든다.
- **레포지토리는 예외를 던지지 않는다.** `guard()`로 감싸 `Either<Failure, T>`를 돌려준다. 오류 문구는 `errorKey` → ARB로 바꾼다.
- **BLE 상수는 `lib/core/constants/ble_protocol.dart` 한 파일에만** 둔다. 펌웨어 `app_config.h`와 값이 1:1로 같아야 한다.
- **문구는 기능별 파일**(`lib/l10n/parts/<기능>_<언어>.arb`)에 넣고 `python3 tool/merge_arb.py && flutter gen-l10n`으로 합친다.
- **비밀값은 BLE로만 스피커에 보낸다.** 휴대폰에는 보안 저장소(KeyVault)에만 기억한다.
- 커밋은 사용자가 요청할 때만 한다.

## 확인 순서 (단계가 끝날 때마다)

```bash
cd flutter
flutter analyze            # 문제 0개
flutter test               # 전부 통과
../scripts/run-flutter.sh android    # 휴대폰에서 실행 (스피커와 같은 Wi-Fi)
# 스크립트가 아직 없으면(12장 12.4절) 대신: flutter run
```

그 장의 "통과 조건"을 하나씩 확인한다. 실기기에서만 확인할 수 있는 항목(권한 창, BLE 연결, mDNS 찾기)은 사용자에게 확인을 부탁한다.

## 더 볼 것

- 장별 프롬프트: [prompts.md](prompts.md)
- 펌웨어 쪽 규격과 규칙은 `esp32-firmware` 스킬, 기획·설계·장애 보고서 양식은 `speaker-method` 스킬
