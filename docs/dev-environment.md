# 개발 환경 설정

이 프로젝트(`firmware/`, `flutter/`)를 빌드하는 데 필요한 도구와 설정 방법입니다.

- 작성일: 2026-10-02
- 기준 컴퓨터: macOS (Apple Silicon), 기본 셸 zsh

## 1. 요약

| 도구 | 버전 | 위치 |
|:--|:--|:--|
| ESP-IDF | **v5.5** | `~/esp/esp-idf-v5.5` |
| ESP-IDF Python 가상환경 | Python 3.14 | `~/.espressif/python_env/idf5.5_py3.14_env` |
| Xtensa 툴체인 (ESP32-S3) | esp-14.2.0_20241119 | `~/.espressif/tools/xtensa-esp-elf/` |
| Python (시스템) | 3.14.6 (Homebrew) | `/opt/homebrew/bin/python3` |
| CMake / Ninja | 4.1.2 / 1.13.1 | |
| Flutter / Dart | 3.38.5 (stable) / 3.10.4 | `~/flutter` |

## 2. ESP-IDF 환경 켜기

새 터미널을 열 때마다 한 번 실행합니다.

```bash
source ~/esp/esp-idf-v5.5/export.sh
```

정상이면 이렇게 끝납니다.

```
Checking "python3" ...
Python 3.14.6
...
Activating ESP-IDF 5.5
Setting IDF_PATH to '/Users/<사용자>/esp/esp-idf-v5.5'.
* Checking python version ... 3.14.6
* Checking python dependencies ... OK
...
Done! You can now compile ESP-IDF projects.
```

확인:

```bash
idf.py --version          # ESP-IDF v5.5
echo $IDF_PATH            # /Users/<사용자>/esp/esp-idf-v5.5
```

"Found tools that are not used by active ESP-IDF version" 안내는 예전 버전 도구가 남아 있다는 뜻이라 무시해도 됩니다. 디스크를 정리하려면 안내에 나온 `idf_tools.py uninstall` 명령을 쓰면 됩니다.

### 편의 별칭 (선택)

`~/.zshrc`의 기존 `get_idf` 별칭은 **다른 버전**(`~/esp/esp-idf` + `idf5.4` 가상환경)을 켭니다. 이 프로젝트에는 쓰지 마세요. 따로 별칭을 두려면:

```bash
alias get_idf55='source ~/esp/esp-idf-v5.5/export.sh'
```

## 3. 주의할 점

### 3.1 python3가 3.14여야 한다

ESP-IDF 5.5 가상환경이 Python 3.14로 만들어져 있어서, `export.sh`가 찾는 `python3`가 3.14여야 합니다.

- 사용자 zsh에서는 `python3` → Homebrew 3.14.6이라 문제없습니다.
- 다른 셸이나 자동화 도구에서 `python3`가 3.12로 잡히면 다음 오류가 납니다.
  ```
  ERROR: ESP-IDF Python virtual environment ".../idf5.5_py3.12_env/bin/python" not found.
  ```
  이때는 zsh 로그인 셸로 실행합니다.
  ```bash
  zsh -lc 'source ~/esp/esp-idf-v5.5/export.sh && cd firmware && idf.py build'
  ```

`~/.zshrc`에 pyenv 초기화 줄이 있습니다. 주석에 적혀 있듯 ESP-IDF를 설치·업그레이드할 때는 pyenv가 방해하므로 그 줄들을 잠시 주석 처리해야 합니다.

### 3.2 ESP-IDF 버전

| 프로젝트 | 빌드한 버전 |
|:--|:--|
| 이 프로젝트 | v5.5 (이 컴퓨터) |
| `waveshare-ai-speaker-assistant` (참고 펌웨어) | v5.5.2 |
| Waveshare 공식 예제 | v5.5.0 |

참고 펌웨어는 v5.5.2로 빌드됐습니다. 지금 v5.5로 시작하되, 드라이버 버그 수정이 필요하면 v5.5.x 최신으로 올립니다. 올릴 때는 이 문서를 고칩니다.

```bash
cd ~/esp/esp-idf-v5.5 && git fetch --tags && git checkout v5.5.2 \
  && git submodule update --init --recursive && ./install.sh esp32s3
```

`~/esp/`에는 `esp-idf`(다른 버전), `esp-idf-v5.3.2-…`도 있습니다. 섞어 쓰지 않도록 항상 `esp-idf-v5.5/export.sh`를 씁니다.

### 3.3 CMake 4.x

PATH의 CMake가 4.1.2입니다. CMake 4는 아주 오래된(3.5 미만) 정책 문법을 지원하지 않아서, 오래된 서드파티 컴포넌트(특히 Spotify Connect 라이브러리)가 빌드되지 않을 수 있습니다. 그런 오류가 나면 해당 컴포넌트의 `cmake_minimum_required`를 올리거나 환경 변수 `CMAKE_POLICY_VERSION_MINIMUM=3.5`를 지정합니다.

## 4. 펌웨어 빌드·업로드

```bash
source ~/esp/esp-idf-v5.5/export.sh
cd firmware
idf.py set-target esp32s3        # 처음 한 번 (sdkconfig 생성)
idf.py build
idf.py -p <포트> flash monitor   # 업로드 + 로그 보기, 종료는 Ctrl+]
```

### 보드 포트 찾기

```bash
ls /dev/cu.usbmodem*
```

보드를 USB-C 데이터 케이블로 연결하면 `/dev/cu.usbmodemXXXX`가 새로 나타납니다. 연결 전후로 목록을 비교해 새로 생긴 것을 쓰세요. (2026-10-02 기준 이 컴퓨터에는 이미 `/dev/cu.usbmodemXXXXXXXXXX`가 있는데, 보드인지 다른 장치인지 확인이 필요합니다.)

다운로드 모드가 필요하면: BOOT 버튼을 누른 채 RESET을 눌렀다 떼고, BOOT를 뗍니다.

### 보드를 출하 상태로 되돌리기

```bash
esptool.py --chip esp32s3 -p <포트> write_flash 0x0 \
  ../ESP32-S3-AUDIO-Board-Demo/Firmware/ESP32-S3-AUDIO-Board.bin
```

## 5. Flutter

```bash
flutter --version     # Flutter 3.38.5 / Dart 3.10.4
cd flutter
flutter pub get
flutter run           # 연결된 휴대폰 또는 시뮬레이터
```

- led-app과 같은 조건(Dart ≥3.10.4, Flutter ≥3.38.4)을 만족합니다.
- iOS 빌드에는 Xcode와 CocoaPods가, Android 빌드에는 Android SDK가 필요합니다. `flutter doctor`로 확인합니다.
- `flutter upgrade` 안내가 나와도, 앱 버전을 고정하기 위해 팀이 정할 때까지 올리지 않습니다.

## 6. 비밀값 관리

- **OTA 서명 키**: `certificates/ota_signing_key.pem` (작업 폴더 최상위). 빌드할 때 이 키로 펌웨어에 서명하므로, **키가 없으면 빌드가 실패한다.** Git으로 관리하므로 저장소를 받으면 함께 받아진다. 대신 **저장소를 공유하면 키도 공유된다** — 원격 저장소는 비공개로 둔다. 잃어버리면 이미 설치된 스피커를 무선(OTA)으로 업데이트할 수 없으니 안전한 곳에 백업한다.

- OpenAI API 키, Telegram 봇 토큰 같은 비밀값은 **스피커에 BLE로 입력**합니다. 펌웨어 소스, `sdkconfig`, Git 저장소에 넣지 않습니다.
- 셸 설정 파일(`~/.zshrc`)에 API 키를 평문으로 두지 않습니다. 필요하면 권한 600인 별도 파일에 두고 `source`하거나 macOS 키체인을 씁니다.
