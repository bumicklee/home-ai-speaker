import 'package:aispeaker/features/assistant/domain/services/place_formatter.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('한국 주소 → 시/도 구/군 동', () {
    test('iOS 형식 (subLocality=구, name=동 번지)', () {
      const p = PlaceParts(
        isoCountryCode: 'KR',
        administrativeArea: '서울특별시',
        subLocality: '강남구',
        thoroughfare: '테헤란로',
        name: '역삼동 736-1',
      );
      expect(PlaceFormatter.format(p), '서울 강남구 역삼동');
    });

    test('Android 형식 (locality=구, subLocality=동, 도로명·건물명은 버린다)', () {
      const p = PlaceParts(
        isoCountryCode: 'KR',
        administrativeArea: '서울특별시',
        locality: '강남구',
        subLocality: '역삼1동',
        thoroughfare: '테헤란로',
        name: '강남파이낸스센터 152',
      );
      expect(PlaceFormatter.format(p), '서울 강남구 역삼1동');
    });

    test('도 + 시 + 구 + 동', () {
      const p = PlaceParts(
        isoCountryCode: 'KR',
        administrativeArea: '경기도',
        locality: '수원시',
        subLocality: '영통구',
        thoroughfare: '매탄동',
        name: '123-4',
      );
      expect(PlaceFormatter.format(p), '경기도 수원시 영통구 매탄동');
    });

    test('광역시·특별자치도 줄임, 읍·면', () {
      expect(
        PlaceFormatter.format(const PlaceParts(
            isoCountryCode: 'KR', administrativeArea: '부산광역시', locality: '해운대구', subLocality: '우동')),
        '부산 해운대구 우동',
      );
      expect(
        PlaceFormatter.format(const PlaceParts(
            isoCountryCode: 'KR', administrativeArea: '제주특별자치도', locality: '서귀포시', subLocality: '남원읍')),
        '제주도 서귀포시 남원읍',
      );
    });

    test('을지로3가처럼 숫자+가인 동', () {
      const p = PlaceParts(isoCountryCode: 'KR', administrativeArea: '서울특별시', subLocality: '중구', name: '을지로3가 12');
      expect(PlaceFormatter.format(p), '서울 중구 을지로3가');
    });

    test('나라 코드가 없어도 시/도 이름으로 한국을 알아본다', () {
      const p = PlaceParts(administrativeArea: '서울특별시', locality: '마포구', subLocality: '서교동');
      expect(PlaceFormatter.format(p), '서울 마포구 서교동');
    });
  });

  test('해외는 동네, 도시, 주, 나라 (번지 없이)', () {
    const p = PlaceParts(
      isoCountryCode: 'US',
      country: 'United States',
      administrativeArea: 'California',
      locality: 'San Francisco',
      subLocality: 'Mission District',
      thoroughfare: 'Valencia St',
      name: '500 Valencia St',
    );
    expect(PlaceFormatter.format(p), 'Mission District, San Francisco, California, United States');
  });

  test('96바이트를 넘으면 뒤를 버린다', () {
    final p = PlaceParts(
      isoCountryCode: 'US',
      subLocality: 'A' * 40,
      locality: 'B' * 40,
      administrativeArea: 'C' * 40,
      country: 'D',
    );
    final s = PlaceFormatter.format(p);
    expect(s, '${'A' * 40}, ${'B' * 40}');
  });

  group('시간대 후보', () {
    test('펌웨어 표에 있으면 그대로', () {
      final t = TimezoneSuggestion.of('America/Los_Angeles');
      expect(t.supported, isTrue);
      expect(t.phone, 'America/Los_Angeles');
    });

    test('표에 없으면 Asia/Seoul·UTC (아시아면 Asia/Seoul 먼저)', () {
      expect(TimezoneSuggestion.of('Asia/Ho_Chi_Minh').fallbacks, ['Asia/Seoul', 'UTC']);
      expect(TimezoneSuggestion.of('Europe/Paris').fallbacks, ['UTC', 'Asia/Seoul']);
      expect(TimezoneSuggestion.of('Europe/Paris').supported, isFalse);
    });

    test('못 읽으면 UTC 먼저', () {
      final t = TimezoneSuggestion.of(null);
      expect(t.supported, isFalse);
      expect(t.fallbacks.first, 'UTC');
    });
  });
}
