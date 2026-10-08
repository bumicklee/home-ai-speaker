#!/usr/bin/env python3
"""lib/l10n/parts/<기능>_<언어>.arb 를 합쳐 lib/l10n/app_<언어>.arb 를 만든다.

기능마다 문구 파일을 따로 두어, 여러 기능을 동시에 만들 때 같은 ARB를 고치며 부딪치지 않게 한다.
키가 겹치면 실패한다. 실행: python3 tool/merge_arb.py && flutter gen-l10n
"""
import json, pathlib, sys

root = pathlib.Path(__file__).resolve().parent.parent / "lib" / "l10n"
langs = {}
for part in sorted((root / "parts").glob("*_*.arb")):
    lang = part.stem.rsplit("_", 1)[1]
    data = json.loads(part.read_text(encoding="utf-8"))
    merged = langs.setdefault(lang, {"@@locale": lang})
    for k, v in data.items():
        if k == "@@locale":
            continue
        if k in merged:
            sys.exit(f"duplicate key {k} in {part.name}")
        merged[k] = v
keysets = {lang: {k for k in m if not k.startswith("@")} for lang, m in langs.items()}
base = keysets.get("ko", set())
for lang, ks in keysets.items():
    if ks != base:
        sys.exit(f"{lang} keys differ from ko: missing {sorted(base - ks)[:5]} extra {sorted(ks - base)[:5]}")
for lang, m in langs.items():
    (root / f"app_{lang}.arb").write_text(json.dumps(m, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("merged", {lang: len(ks) for lang, ks in keysets.items()})
