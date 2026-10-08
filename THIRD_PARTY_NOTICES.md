# Third-party notices

이 저장소의 펌웨어와 자산은 아래 프로젝트를 바탕으로 하거나 참고했습니다. 각 프로젝트의 저작권과 라이선스는 원저작자에게 있습니다.

## waveshare-ai-speaker-assistant

- 출처: https://github.com/KMX415/waveshare-ai-speaker-assistant
- 라이선스: MIT License, Copyright (c) 2026 KMX415
- 가져온 것: 코덱·I2S 초기화, AEC, 재생 큐, 호출어 수명 관리, OpenAI 실시간 대화 처리, 위임·종료 문구 처리
  - 해당 파일 맨 위에 `Based on waveshare-ai-speaker-assistant …` 한 줄을 적었습니다
  - `firmware/components/board`, `audio_core`, `wake`, `live_voice`, `assistant`, `firmware/main/dev_console.c`
- 전문:

```text
MIT License

Copyright (c) 2026 KMX415

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## esphome-waveshare-esp32-s3-audio-va

- 출처: https://github.com/MichalZaniewicz/esphome-waveshare-esp32-s3-audio-va
- 라이선스: MIT License, Copyright (c) 2026 Michał Zaniewicz
- 가져온 것
  - 코드: 없음. 하드웨어 초기화 순서·LED 효과 설계를 참고했습니다 (`docs/HARDWARE.md`, `base/core.yaml`)
  - 소리: `firmware/components/audio_core/assets/startup.mp3`는 이 저장소의 `base/sounds/startup.mp3`와 같은 파일입니다
- 전문:

```text
MIT License

Copyright (c) 2026 Michał Zaniewicz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### startup.mp3 소리의 출처

위 저장소의 README는 소리의 출처로 Home Assistant Voice Preview Edition을 듭니다. Home Assistant Voice Preview Edition의 소리는 다음 조건으로 공개되어 있습니다.

- [Home Assistant Voice Preview Edition Sounds](https://github.com/esphome/home-assistant-voice-pe/tree/dev/sounds) © 2024 [Clayton Charles Tapp](https://www.cctaudio.com/)
- 라이선스: [Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/)

## 이 저장소의 소리 파일

| 파일 | 출처 |
|:--|:--|
| `firmware/components/audio_core/assets/startup.mp3` | 위 esphome-waveshare-esp32-s3-audio-va 항목 참조 |
| `firmware/components/audio_core/assets/ready.mp3` | 저자(이범익)가 준비한 안내음. 이 저장소의 LICENSE(MIT)를 따른다 |

## Waveshare ESP32-S3-AUDIO-Board Demo

- 출처: https://www.waveshare.com/wiki/ESP32-S3-AUDIO-Board (Resources)
- 라이선스: 명시된 라이선스 파일 없음 (Waveshare 위키·제품 페이지의 이용 조건을 따름)
- 가져온 것: 코드 없음. 버튼·LED·MP3 재생 드라이버 구조와 핀 상수를 참고했습니다

## Espressif

- ESP-IDF v5.5: Apache-2.0
- esp-sr 2.1.5: ESPRESSIF MIT License, Copyright (c) 2018 Espressif Systems (Shanghai)
- 그 밖의 관리 컴포넌트: `firmware/dependencies.lock` 참조. 라이선스는 각 컴포넌트 페이지(https://components.espressif.com)에서 확인합니다
- 관리 컴포넌트와 ESP-IDF는 빌드할 때 내려받으며 이 저장소에 들어 있지 않습니다

## Flutter 패키지

- `flutter/pubspec.lock` 참조. 각 패키지의 라이선스는 pub.dev 페이지에서 확인합니다
