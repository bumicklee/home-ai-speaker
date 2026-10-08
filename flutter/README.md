# AI-PA (Flutter)

가족용 AI 스피커의 설정·제어 앱이다. 책 10~18장에서 만든다. 이 책은 Android만 다룬다.

```bash
flutter pub get
flutter analyze
flutter test
../scripts/run-flutter.sh android
```

- 기능 폴더는 `data` · `domain` · `presentation` 세 층이다(10장).
- BLE 상수는 `lib/core/constants/ble_protocol.dart` 한 곳에만 둔다. 펌웨어 `firmware/components/app_config/include/app_config.h`와 값이 같아야 한다(8·12장, 부록 C).
- 문구는 기능별 파일 `lib/l10n/parts/<기능>_<언어>.arb`에 넣고 `python3 tool/merge_arb.py && flutter gen-l10n`으로 합친다.
- 앱 아이콘·스플래시 원본 이미지는 `python3 tool/gen_brand_assets.py`로 만든다(11장).
