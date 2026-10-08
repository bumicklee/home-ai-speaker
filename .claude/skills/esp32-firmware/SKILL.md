---
name: esp32-firmware
description: Waveshare ESP32-S3-AUDIO-Board 스피커 펌웨어(ESP-IDF v5.5) 작업 규칙과 책의 단계별 프롬프트. firmware/ 아래 C 코드, sdkconfig.defaults, partitions.csv를 고치거나, 빌드·굽기·부팅 로그·메모리 부족·호출어·BLE·LAN API·라디오·텔레그램·음성 대화를 다룰 때 쓴다.
when_to_use: 펌웨어, idf.py, ESP-IDF, 부팅 로그, 재부팅, 메모리 부족, 호출어, 컴퓨터/알렉사, BLE 창, LAN API, 라디오, 텔레그램, 절약 모드, Gemini 실시간
paths: firmware/**
argument-hint: "[장 번호, 예: 6]"
---

# 펌웨어 작업 규칙 (책 『클로드 코드로 만드는 우리 집 AI 스피커』)

인자로 장 번호가 오면(`/esp32-firmware 6`) [prompts.md](prompts.md)에서 그 장의 프롬프트를 찾아 순서대로 진행한다. 한 번에 한 단계만 하고, 단계마다 아래 "확인 순서"를 돌린 뒤 사용자에게 결과를 보여 준다.

## 지켜야 할 규칙

- **핀·I2C 주소는 `components/app_config/include/board_config.h`에서만** 가져온다. 다른 파일에 숫자로 쓰지 않는다.
- **아래 모듈은 위 모듈을 모른다.** 아래에서 위로 알릴 일은 `app_events` 이벤트로 보낸다. 버튼은 "눌렸다"만 보내고 뜻은 `main.c`가 정한다.
- **비밀값(AI 키, 봇 토큰)은 BLE로만 받는다.** LAN API로 넣지 않고, 로그에 찍지 않고, 다 쓴 버퍼는 0으로 지운다.
- **설정 옵션은 `sdkconfig.defaults`에 쓴다.** `sdkconfig`는 빌드가 만드는 파일이라 Git에 넣지 않는다. 이미 있으면 `idf.py menuconfig`에서도 바꿔야 반영된다.
- **내부 RAM이 가장 귀하다.** 큰 버퍼·태스크 스택은 PSRAM에 둔다. 새 태스크나 버퍼를 만들면 부팅 로그의 `heap after boot done internal …` 값이 얼마나 줄었는지 보고한다.
- **부가 기능 초기화는 실패해도 부팅을 멈추지 않는다.** `ESP_ERROR_CHECK`는 꼭 필요한 초기화에만 쓴다.
- **OTA 서명 키(`certificates/*.pem`)는 Git에 넣지 않는다.** 저장소가 비공개여도 마찬가지다.
- 모델 이름은 `app_config.h` 상수에만 둔다. 바뀌면 상수만 고친다.
- 커밋은 사용자가 요청할 때만 한다.

## 확인 순서 (단계가 끝날 때마다)

```bash
source ~/esp/esp-idf-v5.5/export.sh
cd firmware && idf.py build                    # 경고 0개를 목표로
idf.py -p <포트> flash monitor                 # 끝낼 때 Ctrl+]
```

1. 부팅 로그에 `E (` 줄이나 `Rebooting...`이 없는지 본다.
2. `heap after boot done internal … (largest …)` 값을 이전과 비교한다.
3. USB 콘솔에서 `selftest`를 실행해 모든 항목이 PASS인지 본다.
4. 그 장의 "통과 조건"을 하나씩 확인한다.

막히면 로그 전체를 그대로 받아 [debugging.md](debugging.md)의 순서로 원인을 좁힌다. 추측으로 고치지 않고, 판단에 쓸 값을 로그로 먼저 찍게 한다.

## 더 볼 것

- 장별 프롬프트: [prompts.md](prompts.md)
- 로그 읽기·장애 분석 순서: [debugging.md](debugging.md)
- 앱 쪽 규칙은 `flutter-app` 스킬, 기획·설계·장애 보고서 양식은 `speaker-method` 스킬
