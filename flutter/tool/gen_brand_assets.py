#!/usr/bin/env python3
"""앱 아이콘·스플래시 이미지를 만든다 (디자인 "앱 아이콘 · 스플래시" 그대로, 1024 좌표계).

    python3 tool/gen_brand_assets.py
    dart run flutter_launcher_icons && dart run flutter_native_splash:create

그림: 흰 고리(스피커 LED 링) + 위쪽 연두 점(대기 LED) + 가운데 세 막대(말소리), 바탕 #0B6E6D.
글꼴 IBM Plex Sans KR(OFL)은 저장소에 넣지 않고 처음 실행할 때 ~/.cache/aipa-fonts 로 받는다.
결과 PNG는 assets/brand/ 에 저장하고 저장소에 커밋한다.
"""
import pathlib
import urllib.request

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "brand"
FONT_DIR = pathlib.Path.home() / ".cache" / "aipa-fonts"
FONT_URL = "https://github.com/google/fonts/raw/main/ofl/ibmplexsanskr/IBMPlexSansKR-{}.ttf"

ACCENT = (0x0B, 0x6E, 0x6D, 255)
LED = (0x9B, 0xE7, 0xC4, 255)
WHITE = (255, 255, 255, 255)
SUBTLE = (0xD6, 0xF0, 0xEE, 255)
SS = 4  # 4배로 그린 뒤 줄여 가장자리를 부드럽게


def glyph(size, *, scale=1.0, background=None, led=True, mono=False):
    """1024 좌표계 그림을 size×size로. scale<1이면 가운데로 줄인다 (적응형 아이콘 안전 영역)"""
    big = size * SS
    img = Image.new("RGBA", (big, big), background or (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    k = big / 1024 * scale
    off = big * (1 - scale) / 2

    def p(v):
        return off + v * k

    # 고리: 중심 512, 반지름 300, 두께 56
    r_out, r_in = (300 + 28) * k, (300 - 28) * k
    c = p(512)
    d.ellipse([c - r_out, c - r_out, c + r_out, c + r_out], fill=WHITE)
    d.ellipse([c - r_in, c - r_in, c + r_in, c + r_in], fill=background or (0, 0, 0, 0))
    # 대기 LED 점 (단색 아이콘에서는 흰색)
    if led:
        rr = 38 * k
        cy = p(212)
        d.ellipse([c - rr, cy - rr, c + rr, cy + rr], fill=WHITE if mono else LED)
    # 막대 세 개: (x, y, 높이), 폭 56, 끝 둥글게
    for x, y, h in ((384, 442, 140), (484, 372, 280), (584, 442, 140)):
        d.rounded_rectangle([p(x), p(y), p(x + 56), p(y + h)], radius=28 * k, fill=WHITE)
    return img.resize((size, size), Image.LANCZOS)


def font(weight, px):
    FONT_DIR.mkdir(parents=True, exist_ok=True)
    path = FONT_DIR / f"IBMPlexSansKR-{weight}.ttf"
    if not path.exists():
        urllib.request.urlretrieve(FONT_URL.format(weight), path)
    return ImageFont.truetype(str(path), px)


def branding(width=800, height=320):
    """스플래시 아래쪽 "AI-PA / 우리 가족 음성 비서" (Android 12 브랜딩 최대 200×80dp = 800×320px @4x)"""
    big = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    title = font("Bold", 88 * SS)
    sub = font("Regular", 52 * SS)
    cx = width * SS / 2
    d.text((cx, height * SS * 0.40), "AI-PA", font=title, fill=WHITE, anchor="mm")
    d.text((cx, height * SS * 0.78), "우리 가족 음성 비서", font=sub, fill=SUBTLE, anchor="mm")
    return big.resize((width, height), Image.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # 앱 아이콘 (iOS·예전 Android): 바탕 꽉 채움, 모서리는 OS가 깎는다
    glyph(1024, background=ACCENT).save(OUT / "icon.png")
    # Android 적응형 아이콘 앞면: 바탕 없이, 안전 영역(지름 66%) 안으로 줄임 → 고리 지름 약 54%
    glyph(1024, scale=0.84).save(OUT / "icon_foreground.png")
    # Android 13 테마 아이콘: 한 가지 색(흰색)만 쓴다
    glyph(1024, scale=0.84, mono=True).save(OUT / "icon_monochrome.png")
    # 스플래시 가운데 그림: 디자인 168dp @4x
    glyph(672).save(OUT / "splash_logo.png")
    # Android 12+ 스플래시 아이콘: 288dp 캔버스(@4x 1152) 안 지름 192dp 원에 그림이 들어가야 한다
    glyph(1152).save(OUT / "splash_android12.png")
    branding().save(OUT / "splash_branding.png")
    for f in sorted(OUT.glob("*.png")):
        print(f"{f.relative_to(ROOT)}  {Image.open(f).size}")


if __name__ == "__main__":
    main()
