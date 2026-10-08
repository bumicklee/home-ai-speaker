import 'dart:async';

import 'package:aispeaker/core/errors/error_keys.dart';
import 'package:aispeaker/core/errors/failures.dart';
import 'package:aispeaker/core/services/device_event_service.dart';
import 'package:aispeaker/features/devices/domain/entities/device_status.dart';
import 'package:aispeaker/features/radio/data/repositories/radio_repository.dart';
import 'package:aispeaker/features/radio/domain/entities/radio_preset.dart';
import 'package:aispeaker/features/radio/domain/radio_rules.dart';
import 'package:aispeaker/features/radio/presentation/bloc/radio_cubit.dart';
import 'package:aispeaker/features/radio/presentation/bloc/radio_search_cubit.dart';
import 'package:dartz/dartz.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class MockRepo extends Mock implements RadioRepository {}

class MockEvents extends Mock implements DeviceEventService {}

const presets = [
  RadioPreset(name: 'CNN', url: 'https://tunein.cdnstream1.com/2868_96.mp3', genre: 'news,talk,english'),
  RadioPreset(name: 'Big B', url: 'https://antares.dribbcast.com/proxy/kpop?mp=/s', genre: 'kpop,korean'),
];
const playing = RadioNow(media: MediaNow(source: 'radio', state: 'playing', station: 'CNN'), preset: 0);

void main() {
  late MockRepo repo;
  late MockEvents events;
  late StreamController<DeviceEvent> stream;

  setUpAll(() => registerFallbackValue(<RadioPreset>[]));

  setUp(() {
    repo = MockRepo();
    events = MockEvents();
    stream = StreamController<DeviceEvent>.broadcast();
    when(() => events.on(DeviceEventType.mediaState))
        .thenAnswer((_) => stream.stream.where((e) => e.type == DeviceEventType.mediaState));
    when(() => repo.presets()).thenAnswer((_) async => const Right(presets));
    when(() => repo.now()).thenAnswer((_) async => const Right(RadioNow()));
    when(() => repo.media(any())).thenAnswer((_) async => const Right(unit));
  });

  RadioCubit build() => RadioCubit(repo, events);

  test('프리셋과 지금 재생을 불러온다', () async {
    when(() => repo.now()).thenAnswer((_) async => const Right(playing));
    final c = build();
    await c.load();
    expect(c.state.loaded, isTrue);
    expect(c.state.presets, presets);
    expect(c.state.playingIndex, 0);
    await c.close();
  });

  test('media_state 이벤트로 머리 카드가 바뀐다', () async {
    final c = build();
    await c.load();
    expect(c.state.now.media.isActive, isFalse);
    stream.add(const DeviceEvent('media_state',
        {'source': 'radio', 'state': 'playing', 'title': 'Song', 'artist': 'A', 'station': 'Big B', 'preset': 1}));
    await Future<void>.delayed(Duration.zero);
    expect(c.state.now.media.state, 'playing');
    expect(c.state.now.media.title, 'Song');
    expect(c.state.playingIndex, 1);
    stream.add(const DeviceEvent('media_state', {'source': 'none', 'state': 'stopped'}));
    await Future<void>.delayed(Duration.zero);
    expect(c.state.playingIndex, isNull);
    await c.close();
  });

  test('프리셋 재생은 번호로 보낸다', () async {
    when(() => repo.playPreset(1)).thenAnswer((_) async => const Right(unit));
    final c = build();
    await c.load();
    await c.playPreset(1);
    verify(() => repo.playPreset(1)).called(1);
    expect(c.state.busy, isFalse);
    await c.close();
  });

  test('검색 결과 지금 틀기는 주소와 이름(48바이트까지)으로', () async {
    when(() => repo.playUrl(any(), any())).thenAnswer((_) async => const Right(unit));
    final c = build();
    await c.playStation(RadioStation(name: '가' * 20, url: 'http://a.example/live'));
    verify(() => repo.playUrl('http://a.example/live', '가' * 16)).called(1);
    await c.close();
  });

  test('재생 중이면 일시정지, 멈춰 있으면 다시 재생', () async {
    when(() => repo.now()).thenAnswer((_) async => const Right(playing));
    final c = build();
    await c.load();
    await c.togglePause();
    verify(() => repo.media('pause')).called(1);

    stream.add(const DeviceEvent('media_state', {'source': 'radio', 'state': 'paused', 'station': 'CNN'}));
    await Future<void>.delayed(Duration.zero);
    await c.togglePause();
    verify(() => repo.media('resume')).called(1);
    await c.close();
  });

  test('409 NOT_PLAYING이면 실패를 알리고 지금 상태를 다시 받는다', () async {
    when(() => repo.media('next')).thenAnswer((_) async => Left(DeviceFailure.fromStatus(409, 'NOT_PLAYING')));
    final c = build();
    await c.load();
    await c.media('next');
    expect(c.state.failure, isA<DeviceFailure>().having((f) => f.code, 'code', 'NOT_PLAYING'));
    verify(() => repo.now()).called(2);
    await c.close();
  });

  test('편집 저장은 목록 전체를 PUT하고 기기가 돌려준 목록을 쓴다', () async {
    final edited = [presets[1].copyWith(name: '케이팝'), presets[0]];
    when(() => repo.savePresets(any())).thenAnswer((i) async => Right(i.positionalArguments[0] as List<RadioPreset>));
    final c = build();
    await c.load();
    expect(await c.savePresets(edited), isTrue);
    verify(() => repo.savePresets(edited)).called(1);
    expect(c.state.presets, edited);
    await c.close();
  });

  test('펌웨어 규칙에 맞지 않으면 보내지 않는다', () async {
    final c = build();
    await c.load();
    expect(await c.savePresets([...presets, const RadioPreset(name: 'x', url: 'rtsp://a/b')]), isFalse);
    expect(c.state.invalid, PresetError.urlInvalid);
    expect(await c.savePresets(List.filled(21, presets[0])), isFalse);
    expect(c.state.invalid, PresetError.tooMany);
    verifyNever(() => repo.savePresets(any()));
    await c.close();
  });

  test('추가는 맨 뒤에, 같은 주소는 거절', () async {
    const added = RadioPreset(name: 'BBC', url: 'https://stream.live.vc.bbcmedia.co.uk/bbc_world_service', genre: 'news');
    when(() => repo.savePresets(any())).thenAnswer((i) async => Right(i.positionalArguments[0] as List<RadioPreset>));
    final c = build();
    await c.load();
    expect(await c.addPreset(added), isTrue);
    verify(() => repo.savePresets([...presets, added])).called(1);
    expect(await c.addPreset(added), isFalse);
    expect(c.state.invalid, PresetError.duplicate);
    await c.close();
  });

  test('저장 실패(400)는 failure로', () async {
    when(() => repo.savePresets(any())).thenAnswer((_) async => Left(DeviceFailure.fromStatus(400, 'INVALID_ARG')));
    final c = build();
    await c.load();
    expect(await c.savePresets(presets), isFalse);
    expect(c.state.failure?.errorKey, ErrorKeys.deviceInvalidArg);
    expect(c.state.presets, presets);
    await c.close();
  });

  group('방송 찾기', () {
    test('이름·장르로 찾고 결과를 보인다', () async {
      const result = [RadioStation(name: 'Jazz FM', url: 'http://jazz.example/live', codec: 'MP3')];
      when(() => repo.search(any(), byTag: any(named: 'byTag'))).thenAnswer((_) async => const Right(result));
      final c = RadioSearchCubit(repo);
      await c.search('  jazz ');
      verify(() => repo.search('jazz', byTag: false)).called(1);
      expect(c.state.results, result);

      c.setByTag(true);
      await Future<void>.delayed(Duration.zero);
      verify(() => repo.search('jazz', byTag: true)).called(1);
      await c.close();
    });

    test('실패하면 결과 없이 failure', () async {
      when(() => repo.search(any(), byTag: any(named: 'byTag')))
          .thenAnswer((_) async => const Left(NetworkFailure(message: 'x')));
      final c = RadioSearchCubit(repo);
      await c.search('jazz');
      expect(c.state.failure, isNotNull);
      expect(c.state.results, isNull);
      await c.close();
    });
  });
}
