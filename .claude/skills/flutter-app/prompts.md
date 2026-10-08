# 앱 단계별 프롬프트

스피커 설정 앱 AI-PA(Flutter, Android)를 만드는 단계별 프롬프트다. 책 부록 K의 앱 프롬프트와, 펌웨어·앱에 함께 쓰는 프롬프트(표에 "공용")를 장 순서대로 그대로 옮겼다. `/flutter-app 12`처럼 장 번호를 주면 그 장의 프롬프트를 순서대로 한 번에 하나씩 진행한다.

다른 파일에 있는 번호: `P08`·`P09`(BLE·LAN 펌웨어) 등 펌웨어 프롬프트와 `PD-`(디버깅)는 `esp32-firmware` 스킬의 prompts.md·debugging.md에, `PR-`(리뷰)와 `T-`(범용 양식)는 `speaker-method` 스킬의 templates.md에 있다.

이 책의 앱은 Android 전용이고 스토어에 올리지 않는다. 릴리즈 APK는 디버그 키로 서명한 채 `adb install`로 설치한다.

## 장 → 프롬프트 번호

| 장 | 프롬프트 |
|:--|:--|
| 1장 개발 환경 | P01-01 (공용), P01-02 (공용) |
| 10장 Flutter 앱 뼈대 | P10-01, P10-02 |
| 11장 디자인 시스템 | P11-01, P11-02, P11-03 |
| 12장 첫 설정 화면과 홈 | P12-01, P12-02, P12-03, P12-04 |
| 13장 화면 5개를 병렬로 | P13-01, P13-02, P13-03 |
| 16장 메모·일정·알림, 영어 선생님 모드 | P16-02, P16-03 (공용) |
| 17장 절약 모드와 AI 고르기 | P17-01 (공용), P17-03 |
| 20장 가족에게 주기 | P20-01 |

## 쓰는 법

- 프롬프트 하나를 붙이고, 끝났다고 하면 **확인 방법**의 명령을 내가 직접 돌린다. 로그는 Claude가 요약한 것 말고 내 터미널에 찍힌 것을 본다.
- 큰 변경(새 컴포넌트, 여러 파일)이면 계획 모드로 계획을 먼저 받아 읽는다.
- 통과하면 `git status`와 `git diff`를 보고 "커밋해 줘"라고 말한다. 커밋은 내가 요청할 때만 한다.
- 실패하면 **막히면**의 요청을 보내기 전에 로그부터 붙인다. 추측으로 고치게 하지 않는다.
- 모든 프롬프트에는 세 가지가 들어 있다. **기준 문서**(설계서 `docs/design.md`를 절 번호가 아니라 주제 이름으로 가리킨다), **바꾸면 안 되는 것**, **확인 방법**.
- 항목 제목 끝의 `· 6장 6.3절`이 쓰는 때다. 조건이 더 있으면 **언제** 줄에 적었다.

| 바꿀 것 | 이 파일의 값 | 바꾸는 법 |
|:--|:--|:--|
| 저장소 | `~/home-ai-speaker` | 프롬프트 안의 경로는 저장소 기준 상대 경로다 |
| 포트 | `/dev/cu.usbmodem*` | 1장에서 찾은 내 포트. Windows는 `COM` 번호 |
| AI 키 | OpenAI 또는 Gemini | 예외 둘: 6장의 USB 콘솔 `key` 명령은 OpenAI 키만 받고, 영어 선생님 모드는 OpenAI 실시간 전용이다 |
| 모델 이름 | `gpt-live-1`, `gemini-3.8-live` 등 | 출판 시점에 바뀔 수 있다. 부록 D를 본다 |
| 휴대폰 | Android | 앱 코드에 iOS 설정이 있어도 고치지 않는다 |

## 1장 개발 환경

1장의 공용 프롬프트(P01-01, P01-02)는 펌웨어 쪽 스킬에도 같은 글로 들어 있다.

### P01-01 설치 오류 해석 · 1장 1.4절
**언제** ESP-IDF·Flutter 설치 중 오류가 날 때마다.
```text
기준 문서: 완성본 저장소(0장 "완성 소스 내려받기")의 ~/home-ai-speaker-final/docs/dev-environment.md(개발 환경 문서).
source ~/esp/esp-idf-v5.5/export.sh 를 했더니 아래 오류가 났다. (오류 전문 붙여넣기)
바꾸면 안 되는 것: ESP-IDF 버전(v5.5)과 설치 위치(~/esp/esp-idf-v5.5). 다른 버전을 설치하라고 제안하지 않는다.
확인 방법: 고친 뒤 idf.py --version 이 ESP-IDF v5.5 를 찍어야 한다.
```
**확인 방법** `idf.py --version`, `flutter doctor`. **막히면** 다른 ESP-IDF가 켜지면(1.5절 3번): "지금 셸에서 어떤 export.sh가 불렸는지 찾는 명령을 알려 주고, 셸 설정 파일에서 다른 버전을 부르는 줄을 찾아 줘. 고치지는 말고 보여만 줘."

### P01-02 저장소와 .gitignore · 1장 1.3절
```text
기준 문서: 이 책 1.4.9절(서명 키).
~/home-ai-speaker 에 빈 Git 저장소를 만들고 .gitignore 를 써 줘. certificates/*.pem, firmware/build/, firmware/sdkconfig, firmware/managed_components/, flutter/build/ 가 제외돼야 한다.
바꾸면 안 되는 것: certificates/ 폴더 자체는 남긴다(.gitkeep). 키 파일은 어떤 경우에도 커밋하지 않는다.
확인 방법: git status 에 .pem 파일이 나오지 않아야 한다. 커밋은 내가 요청할 때만 한다.
```
**확인 방법** `git status`, `git check-ignore -v certificates/ota_signing_key.pem`. **막히면** `firmware/dependencies.lock`이 빠졌으면: "dependencies.lock은 커밋 대상이다. .gitignore에서 이 파일이 제외되지 않게 고쳐 줘."

## 10장 Flutter 앱 뼈대

### P10-01 Flutter 앱 골격 · 10장 10.3절
```text
flutter/ 폴더에 Flutter 앱 골격을 만들어 줘.
- 기준 문서: docs/design.md 의 '기술 스택'(패키지 버전), '폴더 구조', '플랫폼 설정' 절. 패키지는 표에 적힌 메이저 버전을 그대로 쓰고 올리지 마.
- 바꾸면 안 되는 것: BLE 상수는 lib/core/constants/ble_protocol.dart 한 파일에만 두고, 값은 펌웨어 app_config.h 와 설계서의 'BLE GATT 규격' 절과 1:1로 같아야 해. 다른 파일에 UUID를 적지 마.
- 이번에는 화면을 만들지 마. 스플래시와 "등록된 스피커가 없어요" 홈 자리만 둬.
- 확인 방법: flutter analyze 문제 0개, flutter build apk --debug 성공.
```
**확인 방법** `cd flutter && flutter analyze && flutter build apk --debug`. **막히면** 패키지 버전을 올리려 하면 거절한다: "pubspec.yaml의 버전을 설계서 '기술 스택' 표와 한 줄씩 대조해 다른 줄을 되돌려 줘."

### P10-02 BLE·LAN 층과 가짜 스피커 테스트 · 10장 10.3절
```text
core/ble 와 core/network, core/services 를 설계서의 '폴더 구조'대로 만들어 줘.
- 기준 문서: 설계서의 'BLE GATT 규격'(조각 형식 포함), 'LAN API' 절. 펌웨어 ble_cmd.c 의 조각 규칙(순번, 플래그, MTU − 5)을 읽고 똑같이 맞춰.
- 바꾸면 안 되는 것: 레포지토리는 예외를 던지지 말고 Either<Failure, T>를 돌려줘. 모든 Failure는 errorKey를 채워. SSID·비밀번호는 UTF-8로 써.
- 확인 방법: 펌웨어처럼 조각을 받아 합치고 응답을 조각으로 돌려주는 FakeDevice 를 테스트에 만들고, 조각 나누기·재조립, Wi-Fi 스캔 해석, 명령 채널, guard, LAN 클라이언트를 단위 테스트로 통과시켜. 오류 키가 app_ko.arb 와 app_en.arb 둘 다에 있는지도 테스트해.
```
**확인 방법** `flutter test`가 모두 통과. 테스트 이름에 조각·스캔·명령 채널·guard가 보인다. **막히면** 테스트만 통과하고 펌웨어와 어긋날 수 있다: "FakeDevice의 조각 규칙과 firmware/components/ble_prov/ble_cmd.c를 한 줄씩 대조해 다른 점을 표로 보여 줘."

## 11장 디자인 시스템

### P11-01 휴대폰 화면 그리기 (Artifacts) · 11장 11.3절
**언제** 코드보다 먼저. claude.ai 대화창에서 쓴다.
```text
우리 집 AI 스피커를 설정하는 휴대폰 앱의 화면을 HTML 아티팩트로 그려 줘.
- 캔버스: 휴대폰 한 대 = 390×844px. 화면이 여러 개면 가로로 나란히 놓고 각 화면 위에 이름을 붙여.
- 기준 문서: 아래 화면 목록과 각 화면이 하는 일. (여기에 기능표의 "화면 | 하는 일" 표를 붙여 넣는다.)
- 색: 주색은 스피커 대기 LED의 청록을 어둡게 한 것. 흰 글자와 대비 4.5:1 이상이 되는 값을 골라 HEX로 알려 줘. 경고·삭제는 빨강 대신 청록과 어울리는 색으로. 배경은 아주 옅은 회색, 카드는 흰색.
- 글꼴: 한글·영문이 한 가족인 오픈소스 글꼴 하나. 본문 15~16px.
- 누르는 곳은 모두 44px 이상. 목록 한 줄은 56px 이상.
- 바꾸면 안 되는 것: 화면 목록에 없는 기능을 넣지 마. 실제 데이터 대신 "JARVIS_EB1650", "볼륨 12" 같은 예시 값을 써.
- 확인 방법: 다 그린 뒤 색 HEX·대비 비율·글꼴 이름·여백 규칙을 표로 정리해서 아티팩트 아래에 적어 줘. 내가 그 표를 코드로 옮길 거야.
```
**확인 방법** 표의 대비 비율을 대비 계산기로 다시 잰다. 마음에 들 때까지 고치고 HTML을 `docs/`에 저장한다. **막히면** 새 기능(17장 AI 고르기 등)을 넣을 때도 같은 캔버스에 줄을 더해 달라고 한다. 코드보다 그림을 먼저 승인한다.

### P11-02 디자인을 코드로 · 11장 11.3절
```text
디자인 캔버스 "Jarvis 스피커 앱 디자인"(docs/Jarvis 스피커 앱 디자인.html)의 색·글꼴·크기를 lib/core/theme/app_theme.dart 와 lib/core/widgets/app_widgets.dart 로 옮겨 줘.
- 기준: 주색 #0B6E6D(흰 글자 대비 5.9:1), 배경 #F2F4F3, 카드 흰색, 경고 #9A4A07, 글꼴 IBM Plex Sans KR(google_fonts), 누르는 곳 44px 이상.
- 바꾸면 안 되는 것: 색 HEX는 AppColors 상수로만 두고 화면 코드에 직접 적지 마. Material 3 ColorScheme 에 역할별로 꽂고, 어두운 테마도 같은 구조로 만들어. 공통 위젯은 카드·묶음 제목·목록 묶음·목록 줄·스위치 줄·슬라이더 줄·단계 막대·안내 상자·상태 점·실패 화면 10개로 제한하고, 기능 화면은 이것으로만 조립하게 해.
- 확인 방법: flutter analyze 0, 홈 화면(features/home)을 이 위젯으로 다시 만들어 밝은·어두운 테마에서 모두 보이게. 기존 테스트가 그대로 통과해야 해.
```
**확인 방법** `grep -rn "0xFF0B6E6D" flutter/lib/`의 결과가 `app_theme.dart` 한 줄뿐이어야 한다. 주색이 화면 코드에 새지 않았다는 뜻이다. 완성본에도 어두운 카드 위 글자색 같은 `Color(0x…)`가 화면 파일에 몇 개 남아 있으니(11장 11.2절), 그것까지 없애려면 11.7절 과제로 `AppColors`에 옮긴다. **막히면** 화면 코드에 주색 HEX가 남았으면: "features 아래에서 색 상수를 직접 쓴 곳을 찾아 AppColors나 테마 역할로 바꿔 줘."

### P11-03 아이콘과 스플래시 · 11장 11.3절
```text
디자인 캔버스의 "앱 아이콘 · 스플래시" 그대로 아이콘과 스플래시를 만들어 줘.
- 그림: 흰 고리(스피커 LED 링) + 위쪽 연두 점(대기 LED) + 가운데 세 막대(말소리), 바탕 #0B6E6D. 앱 이름은 AI-PA, 스플래시 아래 "AI-PA / 우리 가족 음성 비서".
- 바꾸면 안 되는 것: 이미지는 손으로 그리지 말고 tool/gen_brand_assets.py(Pillow)가 1024 좌표계로 그리게 해. 그림을 바꿀 때 좌표만 고치면 되게. 글꼴 파일은 저장소에 넣지 말고 처음 실행 때 내려받아. 패키지 이름 com.aispeaker.aispeaker 는 건드리지 마.
- 적용은 flutter_launcher_icons 와 flutter_native_splash 로. Android 적응형·단색(Android 13) 아이콘과 Android 12 스플래시 형식까지.
- 확인 방법: 스크립트가 PNG 6장을 assets/brand/ 에 만들고 크기를 출력. dart run flutter_launcher_icons, dart run flutter_native_splash:create 성공. 앱이 스피커 목록을 다 읽을 때까지 스플래시를 유지.
```
**확인 방법** 휴대폰 홈 화면의 아이콘과 이름. **막히면** 적응형 아이콘이 잘리면: "icon_foreground.png의 그림이 안전 영역(가운데 66%) 안에 들어가는지 좌표로 계산해 보여 줘."

## 12장 첫 설정 화면과 홈

### P12-01 첫 설정 상태 기계 · 12장 12.3절
```text
features/provisioning 에 첫 설정 화면을 만들어 줘.
- 기준 문서: 설계서의 '주요 화면 흐름' 절에 있는 "첫 설정" 흐름도와 갈림길(이미 Wi-Fi가 있는 스피커의 유지/바꾸기, 휴대폰 가득 참, 재부팅 뒤 mDNS 재발견 최대 45초). BLE는 10장에서 만든 core/ble 만 쓰고 화면에서 직접 다루지 마. 디자인 캔버스의 첫 설정 화면과 StepProgress 4단계.
- 바꾸면 안 되는 것: 키는 BLE 연결 전에 입력받아. 단계는 ProvStep enum 하나로 관리하고 ProvisioningCubit 이 상태 기계가 되게. LAN에서 못 찾아도 토큰은 저장해. 페이지가 BLE 객체를 들고 있지 않게.
- 확인 방법: bloc_test 로 "키 → 찾기 → 연결 → Wi-Fi 유지 → 등록 → 완료"와 "비밀번호 틀림 → Wi-Fi 목록으로 복귀", "PHONES_FULL → 지울 휴대폰 고르기" 경로를 가짜 BleSpeakerSession 으로 통과시켜.
```
**확인 방법** `flutter test test/features/provisioning/`, 그리고 실기기 첫 시험(12장 12.4절). **막히면** 실기기에서 첫 단계부터 막히면 펌웨어 로그를 같이 본다: "스피커 로그(BLE_PROV·BLE_CMD 줄)와 앱 로그를 시간 순서로 합쳐 어느 단계에서 멈췄는지 보여 줘."

### P12-02 키 기억 · 12장 12.3절
```text
AI 키(OpenAI 또는 Gemini)와 텔레그램 봇 토큰을 휴대폰 보안 저장소에 기억하는 KeyVault 를 core/storage 에 만들어 줘. 첫 설정의 "이 휴대폰에 키 기억하기" 스위치(기본 켬)로 끌 수 있고, 시스템 > 키 관리에서 지울 수 있게. 앱을 지웠다 깔면 InstallState 가 비워.
바꾸면 안 되는 것: 스피커로는 여전히 BLE로만 보내(비밀값은 LAN으로 받지 않는다는 설계 결정). 키를 로그·테스트 출력에 남기지 마.
설계서의 '주요 화면 흐름' 절에 이 결정을 반영해 줘.
확인 방법: 저장·읽기·지우기·재설치 비우기를 단위 테스트로 통과시키고, flutter analyze 0.
```
**확인 방법** `flutter test`. 앱을 지웠다 다시 깔고 첫 설정에서 키 칸이 비어 있는지 본다. **막히면** 키가 LAN 요청에 실리는 코드가 생겼으면 바로 되돌린다: "core/network 아래에서 키·토큰 값을 본문이나 헤더에 넣는 곳을 모두 찾아 줘(휴대폰 토큰 Bearer는 제외)."

### P12-03 홈 화면 · 12장 12.3절
```text
features/home 과 features/devices 로 홈 화면을 만들어 줘. 선택된 스피커의 LAN /status 를 읽어 상태 카드를 그리고, DeviceEventService 의 voice_state·media_state·volume·tg_message 이벤트로 실시간 갱신해.
볼륨 슬라이더는 놓았을 때 보내되 화면은 먼저 바꾸고 실패하면 다시 불러와. 401이면 "이 휴대폰 등록이 해지됨 → 다시 등록" 안내, 연결 불가면 같은 Wi-Fi 확인 안내와 BLE 재설정 버튼.
바꾸면 안 되는 것: 11장의 공통 위젯만 써. 색 HEX를 쓰지 마.
확인 방법: HomeCubit 테스트(정상, 401, 연결 불가, 볼륨 실패 되돌리기)와 flutter analyze 0.
```
**확인 방법** 버튼으로 볼륨을 바꾸면 앱 슬라이더가 따라 움직이는지 본다. **막히면** 실시간 갱신이 안 되면 P09-03의 websocat으로 스피커가 이벤트를 보내는지부터 확인한다.

### P12-04 앱이 스피커를 못 찾을 때 · 12장 12.5절
**언제** 스피커는 광고 중인데 앱 검색에 안 보일 때.
```text
스피커 로그에는 advertising as JARVIS_… 가 있는데 앱에서 "다시 찾기"를 눌러도 안 보인다. 앱 로그를 붙인다. (status=6 등이 보이면 그대로)
기준: flutter/lib/core/ble/ble_link.dart 의 검색 시작·멈춤 코드와 flutter_blue_plus 의 스캔 API, Android의 스캔 시작 횟수 제한.
확인할 것: ① 이미 닫은 화면의 검색이 새 화면의 검색을 멈추는지, ② 짧은 시간에 검색을 여러 번 시작해 제한에 걸리는지.
바꾸면 안 되는 것: 광고 이름 규칙(JARVIS_), 서비스 UUID 필터.
확인 방법: 원인을 코드 줄로 보여 준 뒤 고치고, 검색을 시작한 쪽만 멈출 수 있는지 테스트로 보여 줘.
```
**확인 방법** 만들 때 고친 방법: 검색을 시작한 구독만 멈추고, 한 번 찾기를 30초로, 찾는 중에는 "다시 찾기"를 막았다. **막히면** 그래도 안 보이면 스피커 쪽 P08-04를 본다.

## 13장 화면 5개를 병렬로

### P13-01 병렬 작업 준비 · 13장 13.3절
**언제** 화면 다섯 개를 나누기 전, 메인 세션에서.
```text
라디오·텔레그램·비서·오디오·시스템 화면 다섯 개를 각각 별도 git worktree의 에이전트에게 맡기려 한다. 시작하기 전에 공용 부분을 먼저 준비해 줘.
기준 문서: 설계서의 '폴더 구조' 절, 이 책 13장의 금지 파일 목록.
할 일: ① 기능별 문구 파일 lib/l10n/parts/<기능>_ko.arb·_en.arb 를 tool/merge_arb.py 가 합쳐 app_ko.arb·app_en.arb 를 만들게 하고, ② 기능 입구 파일 규칙(<기능>_feature.dart 가 register<기능>Feature(GetIt sl)과 화면 클래스를 내보냄)을 정하고, ③ 다섯 worktree에 줄 프롬프트를 P13-02 틀로 만들어 줘.
바꾸면 안 되는 것: 기존 화면과 테스트. 이 단계에서는 기능 화면을 만들지 마.
확인 방법: python3 tool/merge_arb.py && flutter gen-l10n 성공, flutter test 그대로 통과.
```
**확인 방법** `git worktree list`로 다섯 폴더가 보이는지. **막히면** 생성 파일(`app_localizations*.dart`)이 부딪힐 것 같으면 금지 파일에 넣고 병합 때 다시 만든다.

### P13-02 화면 하나 (worktree 안에서) · 13장 13.3절
**언제** 각 worktree 세션에 하나씩. 아래는 라디오 예다.
```text
이 worktree에서 라디오 화면만 만들어 줘. 하는 일: 지금 재생, 장르별 프리셋, 재생 제어, 프리셋 편집(순서·이름·삭제·주소 추가), Radio-Browser 방송 찾기.
스코프: lib/features/radio/ 아래에 설계서 '폴더 구조' 절의 구조(data/domain/presentation)로 만들고, 입구 파일 lib/features/radio/radio_feature.dart 에 registerRadioFeature(GetIt sl)과 화면 클래스 RadioPage 를 내보내. 문구는 lib/l10n/parts/radio_ko.arb(기준)와 radio_en.arb 에만 쓰고 키 이름은 radio 로 시작해. 테스트는 test/features/radio/ 에.
금지 파일: lib/injection_container.dart, lib/app/, lib/core/, lib/l10n/app_*.arb, lib/l10n/app_localizations*.dart, 다른 기능의 features/ 와 parts/ 는 고치지 마. 필요한 공용 부품이 core 에 없으면 만들지 말고 "병합 때 필요한 것"으로 마지막 보고에 적어.
규격 출처: 설계서의 'LAN API' 절을 먼저 읽고, 실제 JSON 필드 이름·상태 코드·오류 코드는 펌웨어 코드를 직접 읽어 확인해(firmware/components/local_api/, firmware/components/media_radio/). 설계서와 펌웨어가 다르면 펌웨어를 따르고 다른 점을 보고에 표로 적어.
디자인: 11장 공통 위젯(lib/core/widgets/app_widgets.dart)으로만 조립해. 색 HEX를 쓰지 마. 디자인 캔버스의 라디오 화면을 따라.
완료 조건: flutter analyze 0, python3 tool/merge_arb.py && flutter gen-l10n 성공, flutter test test/features/radio/ 전부 PASS. 끝나면 "앱: 라디오 화면" 한 커밋으로 남기고, 보고에 (1) 만든 파일, (2) 펌웨어에서 확인한 사실(설계서와 다른 점), (3) 병합 때 필요한 것(라우터 경로, DI 호출, 공용 오류 키)을 적어.
```
**확인 방법** 보고의 (2) 표. 만들 때는 `GET /radio/presets`가 `{"presets":[…]}`로 감싸져 온다는 것을 이 단계에서 찾았다. **막히면** 금지 파일을 고쳤으면 그 커밋은 병합하지 않고 되돌리게 한다: "금지 파일 목록에 해당하는 변경을 git diff로 찾아 되돌리고, 필요했던 이유를 보고에 옮겨 줘."

### P13-03 병합 · 13장 13.3절
**언제** 다섯 화면이 끝난 뒤 메인 세션에서.
```text
다섯 worktree의 커밋을 하나씩 병합해 줘. 순서: 오디오 → 시스템 → 비서 → 텔레그램 → 라디오.
기준: 각 worktree 보고의 "병합 때 필요한 것"과 "설계서와 다른 점" 표.
할 일: 병합마다 ① lib/app/ 라우터에 경로 연결, ② injection_container.dart 에 register<기능>Feature 호출, ③ 공용 오류 키(core)가 필요하면 추가, ④ python3 tool/merge_arb.py && flutter gen-l10n 로 생성 파일 다시 만들기.
바꾸면 안 되는 것: 각 기능 폴더의 코드는 병합 충돌이 아니면 고치지 마. 설계서와 다른 점은 설계서를 펌웨어 쪽으로 고친다(펌웨어를 고치지 않는다).
확인 방법: 병합 하나마다 flutter analyze 0, flutter test 전부 통과. 다섯 개가 끝나면 테스트 개수를 알려 줘.
```
**확인 방법** 병합마다 `flutter test`. 실패한 채 다음 병합으로 넘어가지 않는다. **막히면** 생성된 문구 파일이 충돌하면 손으로 풀지 말고 `merge_arb.py`와 `gen-l10n`으로 다시 만든다.

## 16장 메모·일정·알림, 영어 선생님 모드

### P16-02 앱 메모·일정 기능 · 16장 16.3절
```text
docs/design-notes-schedule.md 의 '앱 (AI-PA)' 절과 'LAN API (앱용)' 절(메모·일정 API 규격, 부록 C)을 기준으로 flutter/lib/features/notes/ 를 만들어 줘. 영어 선생님 기능(features/english/)과 같은 Clean Architecture 구조(entities, models, datasources, repository, cubit, pages, registerNotesFeature)로.
바꾸면 안 되는 것: 사본은 앱 문서 폴더 notes_<deviceId>.json 에 {rev, items}로 저장하고 path_provider 를 추가해. 동기화는 앱 열 때·화면 들어올 때·WS notes 이벤트 때 GET /notes?since=rev 로 받아 id별 rev 큰 것을 반영하고 gone 은 사본에서 지워. 앱에서 지우기 전엔 반드시 확인 창("스피커에서도 지워져요"). 409 응답의 "note"를 읽을 수 있게 DeviceApiException 에 응답 본문을 담되 기존 동작은 그대로. 새 오류 코드 FULL, CONFLICT 에 문구를 추가해.
확인 방법: 모델·병합·충돌·알림 칩·cubit 테스트를 쓰고 flutter analyze 문제 없음, flutter test 전부 통과. 다국어는 notes_ko.arb/notes_en.arb.
```
**확인 방법** 스피커에 말로 넣은 일정이 앱에 나타나는지, 앱에서 지운 것이 스피커에서도 사라지는지. **막히면** 충돌(409) 처리가 꼬이면: "같은 항목을 스피커와 앱에서 동시에 고친 경우를 테스트로 만들어 지금 동작을 보여 줘."

### P16-03 영어 선생님 모드 · 16장 16.3절
**언제** OpenAI 키가 필요하다(이 기능은 OpenAI 실시간 전용).
```text
앱 디자인 "영어 선생님" 화면을 기준으로 firmware/components/assistant/lesson.c 와 flutter/lib/features/english/ 를 만들어 줘. 코스 5개(pron/basic/daily/biz/adv), 3분/5분, 수업 중 약 45초마다 전사를 Responses API에 보내 고친 표현·발음·주제를 JSON으로 뽑아 WS lesson 이벤트로 보내고, 끝나면 전체 정리를 /storage/lesson.json 에 저장해.
바꾸면 안 되는 것: 수업은 일반 대화와 같은 live_voice 세션이어야 하고 live_voice 의 바깥 함수는 바꾸지 마. 전사 버퍼는 PSRAM 24KB를 넘지 마. 비서 툴 start_english_lesson 은 지금 대화가 끝난 뒤 수업을 열어야 해.
확인 방법: POST /api/v1/lesson/start {"course":"basic","minutes":3} → 3분 뒤 GET /api/v1/lesson 에 state:"done"과 corrections 배열이 있어야 해. flutter test 의 english 테스트 통과.
```
**확인 방법** 위 curl 두 개. 수업 대화는 `LIVE: limits …`로 시작한다. **막히면** 요금이 걱정되면 수업 길이 상한을 먼저 확인한다. 만들 때 10·20·30분을 3·5분으로 줄였다.

## 17장 절약 모드와 AI 고르기

### P17-01 절약 모드 설계와 타당성 검토 · 17장 17.3절
**언제** 코드보다 먼저.
```text
앱에서 설정으로 '실시간(gpt-live-1)'과 '절약 모드'를 골라 쓰게 하고, 똑같이 Gemini API 키도 쓸 수 있게 하고 싶어. 하루 1시간이면 한 달 약 $90이라 부담이야. docs/design-voice-saver-mode.md 를 써 줘.
OpenAI·Gemini 공식 가격표로 하루 1시간(사람 말 15분, 스피커 말 15분, 왕복 60번) 기준 월 요금을 네 조합 모두 계산하고, 절약 모드가 Waveshare ESP32-S3-AUDIO-Board에서 기술적으로 되는지(VAD, 소리 올리기, Responses API 도구, TTS 재생, 내부 RAM 약 18KB) 항목별로 근거를 적어.
바꾸면 안 되는 것: 지금 도구 28개와 assistant_execute(), 텔레그램 보호(tool_guard), 일정 알림은 그대로 써야 해. 결정은 내가 한다. 질문 목록은 '남은 질문' 절에 따로 남겨.
확인 방법: 각 단가의 출처 페이지를 적고, 추정인 숫자는 "추정"이라고 써.
```
**확인 방법** 단가 출처를 내가 직접 열어 본다. '남은 질문' 절에 내가 답한다. **막히면** 설계서가 결정을 대신 내리면: "결정처럼 쓴 문장을 '남은 질문'으로 옮기고 선택지와 장단점만 남겨 줘."

### P17-03 절약 모드 앱 (작업자 2) · 17장 17.3절
```text
같은 설계서의 '상세 설계' 절 중 '앱 변경' 부분과 디자인 캔버스 "AI 고르기 · 실시간 / 절약 모드" 줄을 기준으로 앱을 고쳐 줘. 첫 설정 1단계를 "① AI 고르기 → 그 AI 키(필수) 또는 '이미 이 AI 키가 저장된 스피커예요' → ② 절약 모드/실시간(월 요금 표시)"로. 새 화면 /assistant/ai(AI 카드 + 방식 카드). 시스템 > 키에 Gemini 키. 홈 상태 카드에 "OpenAI · 절약 모드". 영어 선생님 화면에 "영어 수업은 실시간 API(OpenAI)로 해요 · 분당 약 70원".
바꾸면 안 되는 것: lib/core/ai/ 에 회사·방식 목록과 문구를 모아 여러 화면이 함께 쓰게. 옛 펌웨어처럼 ai 가 없으면 openai/live, available ["openai.live"]로 본다. 키 없음 → 잠금, 키는 있는데 available 에 없음 → "준비 중". 키는 vault_gemini_key 에 기억.
확인 방법: flutter analyze 문제 없음, 설정 모델·ai 해석·BLE gemini_key·첫 설정 잠금·AI 화면 잠금 규칙·영어 키 확인 테스트를 쓰고 flutter test 전부 통과.
```
**확인 방법** `flutter test`. 앱 비서 > AI · 대화 방식에서 잠금이 스피커의 `available`대로 열리고 닫히는지. **막히면** 두 작업자의 JSON 모양이 다르면: "펌웨어 device_info.c·device_settings.c가 실제로 보내는 ai 칸과 앱 lib/core/ai/ai_choice.dart가 읽는 칸을 나란히 표로 보여 줘."

## 20장 가족에게 주기

### P20-01 릴리즈 빌드 스크립트 · 20장 20.3절
**언제** 앱을 가족 휴대폰에 줄 때. 요청문은 20장 20.3절에 실린, 만들 때 쓴 그대로다. 그래서 `ios`·`all` 인자와 AAB가 들어 있다. 고치지 말고 그대로 보낸다.
```text
scripts/build-flutter-release.sh 를 만들어 줘. 인자는 android|ios|all, 옵션 --no-test·--no-clean.
기준 문서: scripts/run-flutter.sh(같은 색·단계 표시 방식), 11장에서 정한 앱 이름(AI-PA).
바꾸면 안 되는 것: flutter/pubspec.yaml 의 version, android/app/build.gradle.kts 의 서명 설정. 빌드 전에 반드시 python3 tool/merge_arb.py 와 flutter gen-l10n 을 돌린다(13장 문구 파일 분리 때문).
확인 방법: 결과물이 releases/ai-pa-<버전>-<날짜>.{apk,aab,ipa} 로 복사되고, 마지막에 설치 명령(adb install -r …)을 출력한다. 디버그 키로 서명됐으면 경고를 보여 준다. 테스트가 하나라도 실패하면 빌드하지 않는다(set -e).
```
**확인 방법** 돌릴 때는 `android` 인자만 쓴다. `./scripts/build-flutter-release.sh android` → `adb install -r releases/ai-pa-….apk`. 스크립트가 AAB를 만들고 `iOS:` 안내 줄도 찍지만 둘 다 쓰지 않는다. 스토어에 올리지 않으므로 릴리즈 키스토어·업로드 키도 필요 없고, 디버그 키 서명 경고는 정상이다. 20장 20.4절 ①의 확인 항목이 이 스크립트 기준이다. **막히면** `[2/5]`에서 멈추면 분석이나 테스트가 실패한 것이다. `--no-test`로 건너뛰지 말고 실패한 테스트 출력을 붙여 원인부터 묻는다.
