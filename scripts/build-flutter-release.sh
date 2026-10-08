#!/bin/bash
# AI-PA 앱 - 릴리즈 빌드
# 사용법: ./scripts/build-flutter-release.sh [android|ios|all] [--no-test] [--no-clean]
#
#   android    Android APK(직접 설치용) + AAB(Play 스토어용)
#   ios        iOS IPA (서명 없이. 배포하려면 Xcode에서 서명)
#   all        둘 다
#   --no-test  빌드 전 정적 분석·테스트를 건너뜀
#   --no-clean flutter clean을 건너뜀 (더 빠름)
#
# 결과물: releases/ai-pa-<버전>-<날짜>.{apk,aab,ipa}
# 휴대폰에 설치: adb install -r releases/ai-pa-….apk

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FLUTTER_DIR="$PROJECT_ROOT/flutter"
OUTPUT_DIR="$PROJECT_ROOT/releases"
APP_NAME="ai-pa"
TARGET="android"
RUN_TESTS=1
CLEAN=1

for arg in "$@"; do
    case $arg in
        android|ios|all) TARGET="$arg" ;;
        --no-test) RUN_TESTS=0 ;;
        --no-clean) CLEAN=0 ;;
        -h|--help)
            sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
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
echo "AI-PA 앱 - 릴리즈 빌드 (${TARGET})"
echo -e "==========================================${NC}"
echo ""

if ! command -v flutter &> /dev/null; then
    echo -e "${RED}오류: Flutter가 설치되어 있지 않습니다.${NC}"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
cd "$FLUTTER_DIR"

echo -e "${CYAN}[1/5] 의존성·문구 준비...${NC}"
if [ "$CLEAN" -eq 1 ]; then
    flutter clean
fi
flutter pub get
python3 tool/merge_arb.py
flutter gen-l10n

if [ "$RUN_TESTS" -eq 1 ]; then
    echo ""
    echo -e "${CYAN}[2/5] 정적 분석·테스트...${NC}"
    flutter analyze
    flutter test --reporter compact
else
    echo ""
    echo -e "${YELLOW}[2/5] 분석·테스트 건너뜀 (--no-test)${NC}"
fi

VERSION=$(grep '^version:' pubspec.yaml | sed 's/version: //' | tr -d ' ')
DATE=$(date +%Y%m%d_%H%M)
BASE="$OUTPUT_DIR/${APP_NAME}-${VERSION}-${DATE}"
echo ""
echo -e "${GREEN}버전: ${VERSION}${NC}"

build_android() {
    echo ""
    echo -e "${CYAN}[3/5] Android APK 빌드...${NC}"
    flutter build apk --release
    cp build/app/outputs/flutter-apk/app-release.apk "${BASE}.apk"
    echo -e "${GREEN}✓ APK $(du -h "${BASE}.apk" | cut -f1) → ${BASE}.apk${NC}"

    echo ""
    echo -e "${CYAN}[4/5] Android AAB 빌드 (Play 스토어용)...${NC}"
    flutter build appbundle --release
    cp build/app/outputs/bundle/release/app-release.aab "${BASE}.aab"
    echo -e "${GREEN}✓ AAB $(du -h "${BASE}.aab" | cut -f1) → ${BASE}.aab${NC}"

    if grep -q 'signingConfigs.getByName("debug")' android/app/build.gradle.kts; then
        echo -e "${YELLOW}  ⚠ 릴리즈 서명 키가 없어 디버그 키로 서명했습니다. 가족 휴대폰에 직접 설치(adb)는 되지만,${NC}"
        echo -e "${YELLOW}    Play 스토어에 올리려면 업로드 키를 만들어 build.gradle.kts의 signingConfig를 바꿔야 합니다.${NC}"
    fi
}

build_ios() {
    echo ""
    echo -e "${CYAN}[3/5] iOS IPA 빌드 (서명 없이)...${NC}"
    if ! command -v xcodebuild &> /dev/null; then
        echo -e "${RED}오류: Xcode가 설치되어 있지 않습니다.${NC}"
        return 1
    fi
    flutter build ipa --release --no-codesign
    IPA=$(ls build/ios/ipa/*.ipa 2>/dev/null | head -1 || true)
    if [ -n "$IPA" ]; then
        cp "$IPA" "${BASE}.ipa"
        echo -e "${GREEN}✓ IPA → ${BASE}.ipa${NC}"
    else
        echo -e "${YELLOW}⚠ IPA 파일이 만들어지지 않았습니다 (서명 필요). Archive: flutter/build/ios/archive/${NC}"
    fi
}

case $TARGET in
    android) build_android ;;
    ios) build_ios ;;
    all)
        build_android
        build_ios
        ;;
esac

echo ""
echo -e "${CYAN}[5/5] 결과${NC}"
echo -e "${GREEN}=========================================="
echo "빌드 완료"
echo -e "==========================================${NC}"
ls -lh "${BASE}".* 2>/dev/null || true
echo ""
echo "설치:"
echo "  Android: adb install -r ${BASE}.apk"
echo "  iOS:     Xcode(Window > Devices and Simulators) 또는 Apple Configurator"
