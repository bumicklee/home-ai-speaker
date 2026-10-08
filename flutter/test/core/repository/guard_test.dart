import 'dart:async';

import 'package:aispeaker/core/errors/error_keys.dart';
import 'package:aispeaker/core/errors/exceptions.dart';
import 'package:aispeaker/core/errors/failures.dart';
import 'package:aispeaker/core/repository/guard.dart';
import 'package:flutter_test/flutter_test.dart';

Future<Failure> failureOf(Object error) async =>
    (await guard<int>(() async => throw error)).fold((f) => f, (_) => throw StateError('expected failure'));

void main() {
  test('성공은 Right', () async {
    expect((await guard(() async => 7)).getOrElse(() => 0), 7);
  });

  test('401 → 등록 해지 키', () async {
    final f = await failureOf(const DeviceApiException(401, null));
    expect(f, isA<DeviceFailure>().having((d) => d.isUnauthorized, 'unauthorized', isTrue));
    expect(f.errorKey, ErrorKeys.deviceUnauthorized);
  });

  test('본문 code가 상태 코드보다 우선', () async {
    expect((await failureOf(const DeviceApiException(409, 'VOICE_ACTIVE'))).errorKey, ErrorKeys.deviceVoiceActive);
    expect((await failureOf(const DeviceApiException(409, 'NO_API_KEY'))).errorKey, ErrorKeys.deviceNoApiKey);
    expect((await failureOf(const DeviceApiException(400, 'INVALID_ARG'))).errorKey, ErrorKeys.deviceInvalidArg);
  });

  test('시간 초과 두 종류 모두 deviceTimeout', () async {
    expect((await failureOf(const DeviceTimeoutException())).errorKey, ErrorKeys.deviceTimeout);
    expect((await failureOf(TimeoutException('x'))).errorKey, ErrorKeys.deviceTimeout);
  });

  test('BLE 오류 키', () async {
    expect((await failureOf(const BleException(BleErrorCode.bondFailed))).errorKey, ErrorKeys.bleBondFailed);
    final f = await failureOf(const BleCommandException('NOT_ALLOWED', data: {'id': 1}));
    expect(f.errorKey, ErrorKeys.bleWindowClosed);
    expect((f as BleFailure).data, {'id': 1});
  });

  test('연결 불가 / 모르는 예외', () async {
    expect((await failureOf(const NetworkException())).errorKey, ErrorKeys.deviceUnreachable);
    expect(await failureOf(StateError('boom')), isA<UnknownFailure>());
  });
}
