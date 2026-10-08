# 펌웨어 디버깅 순서

원칙은 하나다. **고치기 전에 원인을 증거로 정한다.** 이 책에서 원인을 가장 빨리 찾은 때는 늘 진단 로그를 먼저 넣은 때였다. 로그를 받으면 아래 1~4를 차례로 하고, 그래도 원인이 갈리지 않으면 5의 프롬프트(PD-01~PD-04)를 쓴다.

## 1. 로그 받기

```bash
source ~/esp/esp-idf-v5.5/export.sh
cd ~/home-ai-speaker/firmware
idf.py monitor                                   # 로그만 보기
idf.py build flash monitor                       # 새 펌웨어 올리고 로그 보기
idf.py monitor | tee ~/speaker-$(date +%H%M).log # 보면서 파일로 저장
```

- 끝내기는 `Ctrl+]`, 모니터 안에서 스피커 재시작은 `Ctrl+T` 다음 `Ctrl+R`.
- 모니터를 켜면 스피커가 재부팅된다. 재부팅 전 로그는 남지 않으니 **시험 전에 미리 켜 둔다.**
- 화면 복사보다 `tee`로 저장한 파일이 낫다. 붙여넣기는 같은 줄이 두 번 들어가는 일이 잦다.
- 로그가 콘솔 입력 중간에 찍히면 Enter를 한 번 눌러 비운 뒤 명령을 친다.

## 2. 한 줄 읽는 법

```text
I (157094) ASSIST: guard: new user utterance
등급 (부팅 뒤 ms) 태그: 내용
```

| 등급 | 뜻 | 대응 |
|:--|:--|:--|
| `I` | 정보 | 정상 흐름. 읽기만 한다 |
| `W` | 경고. 계획과 다르지만 계속 진행 | **반복되면** 살펴본다 |
| `E` | 오류. 무언가 실패 | **바로 앞뒤 5~10줄**을 함께 본다 |

- 이 빌드는 `CONFIG_LOG_MAXIMUM_LEVEL_INFO=y`라 `D` 줄은 없다.
- 시각은 시계 시간이 아니라 **부팅 뒤 지난 밀리초**다. 두 줄의 차이로 걸린 시간을 본다.
- `---`로 시작하는 줄은 모니터가 붙인 해설(주소 → 함수 이름)이다.
- **오류는 연쇄로 나온다.** 맨 위(가장 먼저 나온) `E` 줄이 원인이고 아래는 결과다. `errno=Success` 같은 말은 결과일 뿐이니 네트워크 문제로 오해하지 않는다.

```text
E (157124) esp-aes: Failed to allocate memory                     ← 원인
E (157124) esp-tls-mbedtls: write error :-0x0001                  ← 결과 1
E (157134) transport_base: esp_tls_conn_write error, errno=Success ← 결과 2
W (157144) GLIVE: Voice connection ended (internal free 8967)     ← 사용자가 느끼는 증상
```

### 태그 지도

| 태그 | 부품 | 이럴 때 본다 |
|:--|:--|:--|
| `MAIN` | 시작 순서, 메모리 측정, 상태 머신 | 부팅 문제, 메모리 |
| `BOARD`, `CODEC`, `ES7210`, `ES8311` | 보드 초기화·마이크·스피커 칩 | 소리가 안 남 |
| `AUDIO` | 마이크·스피커 처리, 에코 제거(AEC) | 소리 크기, 잡음 |
| `WAKE` | 호출어 (Computer 또는 Alexa) | 불러도 반응 없음 |
| `WIFI`, `wifi:`, `SNTP`, `MDNS` | 인터넷·시간·앱이 스피커 찾기 | 연결 끊김 |
| `LIVE` / `GLIVE` / `SAVER` | OpenAI 실시간 / Gemini 실시간 / 절약 모드(두 회사) | 대화가 안 붙거나 끊김 |
| `ASSIST`, `TOOLS` | 도구 실행, 안전장치(guard) | "~해 줘"가 안 됨 |
| `RADIO`, `RADIO_SEARCH`, `PRESETS` | 인터넷 라디오 | 라디오 끊김 |
| `TELEGRAM`, `TG_STORE` | 텔레그램 | 메시지 안 옴·안 감 |
| `NOTES` | 메모·일정 | 메모·알림 |
| `API`, `API_AUTH`, `API_WS`, `BLE_PROV`, `BLE_GATT`, `BLE_CMD`, `PHONES` | 앱과 통신 | 앱 연결 |
| `esp-tls`, `esp-aes`, `websocket_client`, `transport_base` | 암호화 통신(ESP-IDF 라이브러리) | 연결 실패 |

## 3. 빠른 점검 목록 (로그를 받으면 이 순서로)

1. `rst:` — 왜 다시 시작했나.
2. `App version` — 고친 펌웨어가 정말 올라갔나. `-dirty`는 커밋하지 않은 수정이 더해졌다는 뜻이다.
3. `heap after boot done` — internal 12,000 이상인가.
4. `E (`로 검색 — 첫 오류 줄과 그 앞 10줄.
5. `session ended`로 검색 — 대화가 어떻게 끝났나.
6. `W (`로 검색 — 반복되는 경고.
7. `AUDIO: levels` — 32767 근처 값이 있나(클리핑).
8. `underruns`, `dropped` — 0인가.

```bash
grep -nE "^E \(|session ended|could not reserve|internal free" speaker.log
grep -n "App version\|heap after boot done" speaker.log
```

### 재부팅 이유 (`rst:`)

| rst 값 | 뜻 | 판단 |
|:--|:--|:--|
| `0x1 (POWERON)` | 전원을 새로 넣음 | 정상 |
| `0x15 (USB_UART_CHIP_RESET)` | USB(모니터·플래시)가 리셋 | 정상 (모니터 켤 때) |
| `0xc (RTC_SW_CPU_RST)` | 소프트웨어 재시작 (`esp_restart`, 공장 초기화, 패닉 뒤) | 앞에 `Guru Meditation`이 있었는지 본다 |
| `0x7`, `0x8` (`TG0WDT`, `TG1WDT`) | 와치독. 어떤 작업이 너무 오래 멈춤 | 버그. 재시작 전 로그가 필요 |
| `0xf (RTCWDT_BROWN_OUT)` | 전압 부족 | 전원·케이블 문제 |

- 패닉은 `Guru Meditation Error`, `assert failed:`, `ESP_ERROR_CHECK failed:` … `abort() was called`, `E task_wdt: Aborting.` 중 하나로 시작하고, 넷 다 이어서 `Backtrace:`가 나온다. **`Backtrace:`를 찾는다.**
- 이 빌드는 패닉 뒤 곧바로 재부팅하고(`CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y`), 코어 덤프는 껐다. 원인은 재부팅 전 로그의 `Backtrace:` 한 줄이 전부다.
- `E task_wdt: Task watchdog got triggered`는 한 코어의 IDLE 태스크가 10초(`CONFIG_ESP_TASK_WDT_TIMEOUT_S=10`) 동안 못 돌았다는 뜻이다. 어떤 태스크가 CPU를 놓지 않고 계속 돈다. 기다리며 잠든 태스크는 원인이 아니다.
- backtrace가 주소만 있으면 `idf.py monitor`로 다시 띄운다. 모니터가 주소를 함수 이름으로 바꿔 준다.

### 메모리 줄 읽기 (가장 중요)

만들 때 겪은 장애는 대부분 내부 RAM 부족이었다. `MAIN`이 부품을 하나 켤 때마다 남은 메모리를 찍는다.

```text
I (1304) MAIN: heap after start        internal 249375 (largest 122880)  psram 6081564
I (1464) MAIN: heap after audio        internal 176167 (largest  69632)  psram 5834280
I (1564) MAIN: heap after net          internal  54271 (largest  24576)  psram 5709224
I (1904) MAIN: heap after local_api    internal  13499 (largest   7680)  psram 2687208
I (1954) MAIN: heap after boot done    internal  13371 (largest   7680)  psram 2686592
```

| 값 | 뜻 | 판단 |
|:--|:--|:--|
| `internal` | 내부 RAM 총 여유 (바이트) | 12,000 이상: 보통 / 10,000 아래: TLS 연결 실패 위험 / 8,000대: 실제 실패가 남 |
| `largest` | 여유 중 가장 큰 한 덩어리 | 무언가를 빌릴 때는 한 덩어리로 줘야 한다. 총량이 커도 이게 작으면 실패(조각남) |
| `psram` | PSRAM 여유 | 1MB 이상이면 충분 |

- 위 숫자는 18장까지 들어간 펌웨어의 예시다. 숫자는 펌웨어마다 다르다. 새 기능을 넣으면 **이전 펌웨어의 같은 줄과 비교**하고 작업 기록에 적는다(PD-04).
- 기준선은 **부팅 끝 `internal` 12,000 이상**이다.
- `MAIN: %s init failed: %s (feature disabled)`는 부가 기능 하나가 꺼지고 부팅은 계속됐다는 경고다. 어느 기능인지 기록한다.

| 메모리가 나오는 다른 줄 | 뜻 |
|:--|:--|
| `AUDIO: AEC uses 90912 B internal RAM` | 에코 제거가 내부 RAM 약 89KB를 씀 (가장 큰 소비자) |
| `GLIVE/LIVE: Voice connection ended (internal free N)` | 대화 연결이 끊긴 순간의 여유. N이 10,000 아래면 메모리를 의심 |
| `LIVE: could not reserve 12800 B for the voice task (largest internal N)` | 대화 태스크 자리를 미리 못 맡음. 조각남. 라디오를 켠 뒤 흔하다 |
| `TELEGRAM: bot connected (stack free min N B)` | 텔레그램 태스크 스택의 최소 여유 |
| `MAIN: BLE window: internal free %u (largest %u)` / `AUDIO: AEC suspended (internal free %u)` | BLE 창 열기 직전 / AEC를 내린 뒤 |

### 무시해도 되는 줄

| 줄 | 이유 |
|:--|:--|
| `E I2S_IF: Mode 1 conflict sample_rate 16000 with 0` | 스피커 칩을 마이크와 같은 I2S 통로에 붙일 때 라이브러리가 찍는다. 부팅음이 정상으로 들리면 기능 문제가 아니다 |
| `W CONSOLE: developer console enabled (disable CONFIG_APP_DEV_CONSOLE for release)` | 개발 중엔 정상. 출시 빌드에서 끈다 |
| `main_task: Returned from app_main()` | 시작 순서가 끝났다는 뜻. 일은 다른 태스크가 계속한다 |
| `MAIN: ready, but no OpenAI key: no announcement` | 지금 코드는 OpenAI 키만 확인해 안내음을 생략한다. Gemini 키만 넣은 스피커에서도 나온다 |

### 자주 보는 오류 줄

| 오류 줄 | 뜻 | 할 일 |
|:--|:--|:--|
| `ESP_ERROR_CHECK failed: esp_err_t 0x101 (ESP_ERR_NO_MEM)` … `Rebooting...` | 내부 RAM 부족으로 초기화 실패, 재부팅 반복 | `heap after` 줄로 어느 단계에서 모자라는지 찾는다. `sdkconfig.defaults`의 PSRAM 옵션(`CONFIG_SPIRAM_XIP_FROM_PSRAM=y` 등) 확인 (P07-01) |
| `E esp-aes: Failed to allocate memory` | 암호화 하드웨어가 내부 RAM 임시 버퍼를 못 빌림 | 부팅 직후부터면 `heap after boot done`이 10,000 아래인지(P18-03). 라디오를 켠 채 대화 중이면 조각남(P18-05). `sdkconfig`에 `# CONFIG_MBEDTLS_HARDWARE_AES is not set`이 있는지 |
| `E esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x7F00` | 연결 협상 중 메모리 부족 | 내부 RAM 확인 |
| `E esp-tls: couldn't get hostname … getaddrinfo() returns 202` | DNS 실패 | 예비 DNS로 넘어가는지, 인터넷 연결 자체를 본다 |
| `E websocket_client: Error create websocket task` | 대화 연결 태스크를 만들 연속 블록이 없음(조각남). 인터넷 문제 아님 | P07-05 |
| `E BLE_INIT: Malloc failed` → `nimble init: ESP_ERR_NO_MEM` | BLE 컨트롤러용 내부 RAM 부족 | 창을 열기 전에 `AEC suspended`가 먼저 나오는지 (P08-04) |
| `W GLIVE: Voice startup timed out (internal free N)` | 연결 뒤 20초 안에 `setup complete` 없음 | `server closed` 줄을 찾는다 (P18-04) |
| `W GLIVE/LIVE: Speaker backlog (internal free N)` | 재생 큐 넘침 | P18-05 |
| `***ERROR*** A stack overflow in task X` | 태스크 X의 스택이 모자람 | 그 태스크 스택 늘리기. `Tmr Svc`면 `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=4096` 확인 |
| `SAVER: stt http 401` / `SAVER: gemini http 401`·`403` | AI 키 문제 (Gemini는 `x-goog-api-key` 헤더도 의심) | 키 확인 |
| `SAVER: gemini http 400` | 요청 형식 문제 (도구 형식 등) | P18-01의 막히면 |
| `TELEGRAM: poll failed (HTTP 409)` → `state conflict` | 같은 봇 토큰을 다른 프로그램이 씀 | 다른 프로그램 끄기 |
| `RADIO: unsupported playlist (HLS?)` | HLS 방송 | P14-04 |
| `WAKE: wake model failed to load` | 모델 파티션 비었음 | `idf.py -p <포트> flash`로 전체 굽기 |
| `WAKE: … peak`가 수백 | 마이크 소리 너무 작음 | P07-04. 콘솔 `mic 36`, `wakegain 12` |

## 4. 증상 → 먼저 볼 줄 → 프롬프트

| 증상 | 먼저 볼 줄 | 프롬프트 (prompts.md) |
|:--|:--|:--|
| 첫 부팅에서 재부팅 반복 | `ESP_ERR_NO_MEM`, `heap after …` | P07-01 → P07-02 → P07-03 |
| `talk`는 되는데 호출어만 안 됨 | `WAKE: listening …`, `AUDIO: levels`, `WAKE: stats` | P07-04 |
| 오래 켜 둔 뒤 대화가 안 붙음 | `Error create websocket task`, `could not reserve` | P07-05 |
| 설정 끝난 스피커에서 BLE 창이 안 열림 | `BLE_INIT: Malloc failed` | P08-04 |
| 라디오가 끊기며 나옴 | `RADIO: net … underruns …` | P14-01의 막히면, PD-02 |
| 덕킹 때 "지지직" | `RADIO: … underruns +0` | P14-05 |
| 텔레그램 답장이 막힘 | `TOOLS: run telegram_send`, `ASSIST: guard: … blocked` | P15-04 |
| 대화가 무응답 시간을 넘겨도 안 끝남 | `LIVE: limits`, `LIVE: idle …` | P15-05 |
| 메모를 넣으면 재부팅 | `task_wdt`, `Backtrace:` | P16-04 |
| 절약 모드가 말을 못 알아들음 | `SAVER: limits`, `SAVER: listen 5 s: …` | P17-04 |
| 새 엔진을 넣었더니 TLS 실패 | `esp-aes: Failed to allocate memory`, `heap after boot done` | P18-03 |
| Gemini 연결 뒤 20초에 끊김 | `GLIVE: Voice startup timed out`, `server closed` | P18-04 |
| 라디오를 켠 채 대화하면 끊김 | `esp-aes …` 또는 `Speaker backlog` | P18-05 |
| 위에 없는 증상 | 첫 `E` 줄과 앞뒤 | PD-01 → PD-02 |

## 로그를 붙일 때

- **문제가 난 시각 앞뒤 30초**를 포함한다. 오류 줄만 붙이면 원인을 못 찾는다.
- 재부팅이 있었으면 **재부팅 전 로그**(`Guru Meditation`, `Backtrace`)가 중요하다.
- 무엇을 했는지 한 줄 적는다. "라디오 켠 채로 텔레그램 보내 달라고 함" 같은 한 줄이 시간을 크게 줄인다.
- 펌웨어는 키·토큰·대화 내용·메시지 본문·라디오 주소·Wi-Fi 비밀번호를 찍지 않게 만들었다. 그래도 붙이기 전에 한 번 훑는다. 키를 친 콘솔 화면은 그 줄을 지우고 붙인다.

## 5. 디버깅·RCA 프롬프트 (재사용)

### PD-01 로그 붙이고 원인 후보 세우기
```text
로그를 붙인다. 타임스탬프와 앞뒤 줄을 지우지 않았다. (로그)
아직 고치지 마. 할 일:
1) 이 로그에서 정상으로 확인되는 것과 비정상인 줄을 나눠 표로.
2) 원인 후보를 셋 이상 세우고, 후보마다 "이 원인이라면 로그에 있어야 할 줄 / 없어야 할 줄"을 적어 실제 로그와 대조해.
3) 로그만으로 가를 수 없으면, 가르는 데 필요한 진단 로그 한두 줄을 제안해(값의 내용·키·URL은 찍지 않는다).
4) 지운 후보는 "무엇이 아니었나" 표(의심 | 아닌 이유 | 근거 로그 줄)로 남겨. 근거 줄이 없으면 "아직 지우지 못함".
기준: 설계서의 관련 절과 해당 소스 파일. 추정이면 "추정"이라고 써.
```
**확인 방법** 후보마다 근거 로그 줄이 붙어 있는지 본다. 18장 장애 B가 이 표로 키·인터넷·메모리·설정 거절을 한 줄씩 지운 예다.

### PD-02 진단 로그 먼저 넣기
```text
(증상 한 줄). 원인을 추측하지 말고 판단에 쓰는 값을 로그로 보이게 해 줘.
넣을 것: 이 기능이 "예/아니오"를 판단할 때 쓰는 값(문턱값, 크기, 횟수, 남은 시간), 시작 때 설정값 한 줄, 일정 주기 요약 한 줄.
바꾸면 안 되는 것: 동작. 로그만 더한다. 내용·키·URL·메시지 본문은 찍지 않는다. 주기는 입력을 방해하지 않게 길게.
확인 방법: 다음 실행 로그 한 번으로 후보 중 무엇이 맞는지 가를 수 있어야 한다. 어떤 숫자가 나오면 어떤 원인인지 표로 미리 적어 줘.
```
**확인 방법** 이 책의 예: `LIVE: idle …`(P15-05. 같은 모양의 `GLIVE: idle …`은 18장), `SAVER: listen 5 s: …`(17장), `RADIO: net …`(14장), `MAIN: heap after …`(7장).

### PD-03 RCA 문서 쓰기
```text
docs/rca_<슬러그>.md 에 원인 분석 문서를 써 줘. 기준: 이 책 19장의 RCA 양식(문제 하나면 양식 A, 여러 개가 얽혔으면 양식 B).
머리: 작성일, 대상 펌웨어 커밋(git rev-parse --short HEAD 결과), 상태 한 줄.
양식 A: 증상(로그 그대로, 전후 5줄) | 직접 원인 | 근본 원인 | 키운 원인 | 영향 | 조치 | 남은 일. 그 뒤 시간 순서, 무엇이 아니었나, 왜 개발 중에 못 잡았나, 확인 방법(기대 로그), 재발 방지 표.
바꾸면 안 되는 것: 직접 원인과 근본 원인을 같은 문장으로 쓰지 마. 상태는 "빌드 통과"와 "✅ 실기기 확인"을 구분해. 내가 실기기에서 보지 않은 것은 "실기기 확인 필요"로 둬.
확인 방법: 재발 방지 표에 "로그를 추가했다" 또는 "셀프테스트에 넣었다"가 적어도 한 줄.
```
**확인 방법** 코드를 고치기 전에 쓰기 시작한다. 고친 뒤 상태 열만 갱신한다.

### PD-04 메모리 비교
```text
새 기능을 넣은 펌웨어와 이전 펌웨어의 부팅 로그를 붙인다.
MAIN: heap after … 줄을 단계별로 나란히 표로 만들고(internal, largest, psram), 차이가 난 단계와 그 단계에서 새로 들어간 코드를 연결해 줘.
내부 RAM을 쓰는 후보(static 배열, 기본 큐·세마포어, 내부 RAM 태스크 스택, 라이브러리 할당)를 크기와 함께 표로.
바꾸면 안 되는 것: 아직 고치지 마.
확인 방법: 부팅 끝 internal 이 경고선(12,000) 위인지, largest 가 대화에 필요한 덩어리보다 큰지 함께 적어 줘.
```
**확인 방법** 새 기능마다 이 표를 작업 기록에 남긴다. 7장·18장의 교훈이다.

RCA 문서를 고친 뒤 마무리하는 양식(T-06)과 리뷰 양식(PR-01·PR-02)은 `speaker-method` 스킬의 templates.md에 있다.
