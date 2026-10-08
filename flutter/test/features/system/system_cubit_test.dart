import 'package:aispeaker/core/errors/error_keys.dart';
import 'package:aispeaker/core/errors/failures.dart';
import 'package:aispeaker/features/system/domain/entities/system_status.dart';
import 'package:aispeaker/features/system/domain/repositories/system_repository.dart';
import 'package:aispeaker/features/system/presentation/bloc/system_cubit.dart';
import 'package:dartz/dartz.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class MockRepo extends Mock implements SystemRepository {}

const status = SystemStatus(firmware: '0.5.0', uptime: Duration(minutes: 5), wifiRssi: -51);
const me = RegisteredPhone(phoneId: 'p1', name: '엄마 Galaxy', self: true);
const dad = RegisteredPhone(phoneId: 'p2', name: '아빠 iPhone');

void main() {
  late MockRepo repo;

  setUp(() {
    repo = MockRepo();
    when(() => repo.status()).thenAnswer((_) async => const Right(status));
    when(() => repo.phones()).thenAnswer((_) async => const Right([me, dad]));
  });

  test('load: 상태와 휴대폰 목록, self 찾기', () async {
    final c = SystemCubit(repo);
    await c.load();
    expect(c.state.status, status);
    expect(c.state.phones, [me, dad]);
    expect(c.state.self, me);
    expect(c.state.failure, isNull);
  });

  test('load: 상태 실패해도 휴대폰 목록 실패와 따로 둔다', () async {
    when(() => repo.status()).thenAnswer((_) async => const Left(NetworkFailure()));
    final c = SystemCubit(repo);
    await c.load();
    expect(c.state.status, isNull);
    expect(c.state.failure, const NetworkFailure());
    expect(c.state.phones, [me, dad]);
  });

  test('revoke: 성공하면 목록에서 뺀다', () async {
    when(() => repo.revokePhone('p2')).thenAnswer((_) async => const Right(unit));
    final c = SystemCubit(repo);
    await c.load();
    expect(await c.revoke(dad), isNull);
    expect(c.state.phones, [me]);
    expect(c.state.busy, isFalse);
  });

  test('revoke: 404면 이미 없는 것 — 실패로 보지 않는다', () async {
    when(() => repo.revokePhone('p2'))
        .thenAnswer((_) async => Left(DeviceFailure.fromStatus(404, 'NOT_FOUND')));
    final c = SystemCubit(repo);
    await c.load();
    expect(await c.revoke(dad), isNull);
    expect(c.state.phones, [me]);
  });

  test('revoke: 다른 실패는 돌려주고 목록은 그대로', () async {
    when(() => repo.revokePhone('p2')).thenAnswer((_) async => const Left(NetworkFailure()));
    final c = SystemCubit(repo);
    await c.load();
    expect(await c.revoke(dad), const NetworkFailure());
    expect(c.state.phones, [me, dad]);
  });

  test('reboot: 대화 중이면 409 VOICE_ACTIVE → 대화 중 문구 키', () async {
    when(() => repo.reboot()).thenAnswer((_) async => Left(DeviceFailure.fromStatus(409, 'VOICE_ACTIVE')));
    final c = SystemCubit(repo);
    final f = await c.reboot();
    expect(f?.errorKey, ErrorKeys.deviceVoiceActive);
    expect(c.state.busy, isFalse);
  });

  test('reboot: 성공', () async {
    when(() => repo.reboot()).thenAnswer((_) async => const Right(unit));
    expect(await SystemCubit(repo).reboot(), isNull);
  });

  test('revokeSelf: 목록을 아직 안 불렀으면 불러서 이 휴대폰을 해지', () async {
    when(() => repo.revokePhone('p1')).thenAnswer((_) async => const Right(unit));
    final c = SystemCubit(repo);
    expect(await c.revokeSelf(), isTrue);
    verify(() => repo.revokePhone('p1')).called(1);
  });

  test('revokeSelf: 연결이 안 되면 false (지우기는 계속)', () async {
    when(() => repo.phones()).thenAnswer((_) async => const Left(NetworkFailure()));
    expect(await SystemCubit(repo).revokeSelf(), isFalse);
    verifyNever(() => repo.revokePhone(any()));
  });
}
