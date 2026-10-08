import 'dart:convert';

import 'package:aispeaker/core/ble/ble_frame.dart';
import 'package:aispeaker/core/constants/ble_protocol.dart';
import 'package:aispeaker/core/errors/exceptions.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('BleFrameCodec.encode', () {
    test('한 조각이면 첫·마지막 플래그가 둘 다 켜진다', () {
      final frames = BleFrameCodec.encode(utf8.encode('{"id":1}'), 20);
      expect(frames, hasLength(1));
      expect(frames[0][0], 0);
      expect(frames[0][1], BleProtocol.flagFirst | BleProtocol.flagLast);
      expect(utf8.decode(frames[0].sublist(2)), '{"id":1}');
    });

    test('여러 조각: seq 증가, 첫/마지막 플래그, payload 크기', () {
      final msg = List<int>.generate(50, (i) => i);
      final frames = BleFrameCodec.encode(msg, 20);
      expect(frames.map((f) => f[0]), [0, 1, 2]);
      expect(frames.map((f) => f[1]), [BleProtocol.flagFirst, 0, BleProtocol.flagLast]);
      expect(frames.map((f) => f.length - 2), [20, 20, 10]);
      expect(frames.expand((f) => f.sublist(2)).toList(), msg);
    });

    test('4096바이트 초과는 거부', () {
      expect(() => BleFrameCodec.encode(List.filled(4097, 0), 200), throwsA(isA<BleException>()));
      expect(BleFrameCodec.encode(List.filled(4096, 0), 200), hasLength(21));
    });

    test('256조각 초과는 거부 (seq 래핑 금지)', () {
      expect(() => BleFrameCodec.encode(List.filled(257, 0), 1), throwsA(isA<BleException>()));
    });
  });

  group('BleFrameReassembler', () {
    test('나눈 것을 다시 합친다', () {
      final msg = utf8.encode(jsonEncode({'id': 3, 'ok': true, 'token': 'x' * 300}));
      final r = BleFrameReassembler();
      final frames = BleFrameCodec.encode(msg, 182);
      for (final f in frames.take(frames.length - 1)) {
        expect(r.add(f), isNull);
      }
      expect(r.add(frames.last), msg);
    });

    test('순서가 틀리면 오류, 다음 첫 조각부터 다시 받는다', () {
      final r = BleFrameReassembler();
      final frames = BleFrameCodec.encode(List.filled(30, 7), 10);
      r.add(frames[0]);
      expect(() => r.add(frames[2]), throwsA(isA<BleException>()));
      expect(r.add(frames[0]), isNull);
      expect(r.add(frames[1]), isNull);
      expect(r.add(frames[2]), List.filled(30, 7));
    });

    test('첫 조각 없이 온 조각은 오류', () {
      expect(() => BleFrameReassembler().add([1, 0, 9]), throwsA(isA<BleException>()));
    });
  });
}
