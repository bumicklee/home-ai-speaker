#!/bin/bash
# AI-PA 앱 - 휴대폰에서 빌드·실행
# 사용법: ./scripts/run-flutter.sh [android|ios] [--release|--profile]
#
#   android    Android 휴대폰에서 실행
#   ios        iPhone에서 실행
#   (생략)     연결된 휴대폰을 자동으로 고름. 둘 다 있으면 묻는다
#   --release  릴리즈 모드 (USB를 빼도 앱이 그대로 동작)
#   --profile  프로파일 모드 (성능 측정)
#   기본       디버그 모드 (핫 리로드 가능, USB 연결 유지)
#
# 휴대폰과 스피커가 같은 Wi-Fi에 있어야 첫 설정 뒤 LAN 연결이 된다.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."
FLUTTER_DIR="$PROJECT_ROOT/flutter"
TARGET=""
BUILD_MODE="debug"

for arg in "$@"; do
    case $arg in
        android) TARGET="android" ;;
        ios) TARGET="ios" ;;
        --release|-r) BUILD_MODE="release" ;;
        --profile|-p) BUILD_MODE="profile" ;;
        -h|--help)
            sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "알 수 없는 인자: $arg (도움말: $0 --help)"
            exit 1
            ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${BLUE}=========================================="
echo "AI-PA 앱 - 휴대폰에서 실행"
echo -e "==========================================${NC}"
echo ""

if ! command -v flutter &> /dev/null; then
    echo -e "${RED}오류: Flutter가 설치되어 있지 않습니다.${NC}"
    exit 1
fi
flutter --version | head -1
echo ""

cd "$FLUTTER_DIR"

echo -e "${CYAN}[1/4] 의존성·문구 준비...${NC}"
flutter pub get
# 기능별 문구(lib/l10n/parts)를 합쳐 생성 파일을 최신으로
python3 tool/merge_arb.py
flutter gen-l10n

echo ""
echo -e "${CYAN}[2/4] 연결된 휴대폰 확인...${NC}"
# flutter devices --machine (JSON)으로 실제 기기만 고른다 (에뮬레이터·데스크톱·웹 제외)
DEVICES_JSON=$(flutter devices --machine 2>/dev/null || echo "[]")
read -r ANDROID_ID ANDROID_NAME IOS_ID IOS_NAME <<< "$(python3 - "$DEVICES_JSON" <<'PY'
import json, sys
try:
    devices = json.loads(sys.argv[1])
except ValueError:
    devices = []
def pick(platform_prefix):
    for d in devices:
        if d.get("emulator"):
            continue
        if d.get("targetPlatform", "").startswith(platform_prefix):
            return d["id"], d.get("name", "").replace(" ", "_")
    return "-", "-"
a = pick("android")
i = pick("ios")
print(a[0], a[1], i[0], i[1])
PY
)"

[ "$ANDROID_ID" = "-" ] && ANDROID_ID=""
[ "$IOS_ID" = "-" ] && IOS_ID=""
[ -n "$ANDROID_ID" ] && echo -e "${GREEN}Android: ${ANDROID_NAME//_/ } (${ANDROID_ID})${NC}"
[ -n "$IOS_ID" ] && echo -e "${GREEN}iOS: ${IOS_NAME//_/ } (${IOS_ID})${NC}"

if [ -z "$ANDROID_ID" ] && [ -z "$IOS_ID" ]; then
    echo -e "${RED}오류: 연결된 휴대폰이 없습니다.${NC}"
    echo ""
    echo "연결 방법:"
    echo "  Android: 개발자 옵션 > USB 디버깅 켜기 → USB 연결 → 휴대폰에서 '허용'"
    echo "  iOS: Xcode에서 기기 등록·신뢰 → USB 연결 (Xcode > Settings > Components에 iOS 플랫폼 필요)"
    echo ""
    flutter devices
    exit 1
fi

if [ -z "$TARGET" ]; then
    if [ -n "$ANDROID_ID" ] && [ -n "$IOS_ID" ]; then
        echo -e "${YELLOW}Android와 iPhone이 모두 연결되어 있습니다. 고르세요:${NC}"
        echo "  1) Android"
        echo "  2) iOS"
        read -r -p "선택 [1-2]: " choice
        case $choice in
            2) TARGET="ios" ;;
            *) TARGET="android" ;;
        esac
    elif [ -n "$ANDROID_ID" ]; then
        TARGET="android"
    else
        TARGET="ios"
    fi
fi

case $TARGET in
    android) DEVICE_ID="$ANDROID_ID" ;;
    ios) DEVICE_ID="$IOS_ID" ;;
esac
if [ -z "$DEVICE_ID" ]; then
    echo -e "${RED}오류: ${TARGET} 기기가 연결되어 있지 않습니다.${NC}"
    exit 1
fi

MODE_FLAG=""
[ "$BUILD_MODE" = "release" ] && MODE_FLAG="--release"
[ "$BUILD_MODE" = "profile" ] && MODE_FLAG="--profile"

echo ""
echo -e "${CYAN}[3/4] 빌드 모드: ${BUILD_MODE}${NC}"
if [ "$BUILD_MODE" = "release" ] && [ "$TARGET" = "android" ]; then
    echo -e "${YELLOW}  참고: 릴리즈 서명 키가 아직 없어 디버그 키로 서명합니다 (android/app/build.gradle.kts).${NC}"
fi

echo ""
echo -e "${CYAN}[4/4] 앱 실행 (${TARGET}: ${DEVICE_ID})...${NC}"
echo ""
echo "  핫 리로드: r   핫 리스타트: R   종료: q"
echo ""
flutter run -d "$DEVICE_ID" $MODE_FLAG
