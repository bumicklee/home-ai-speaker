import 'package:dartz/dartz.dart';

import '../../../../core/errors/failures.dart';
import '../entities/system_status.dart';

/// 시스템 화면: 상태, 등록된 휴대폰, 재시작, 스피커의 키 (설계서 2.13–2.14절)
abstract interface class SystemRepository {
  /// 선택된 스피커의 기기 ID (없으면 null)
  String? get deviceId;

  /// IP 주소 (화면 표시용)
  String? get host;

  Future<Either<Failure, SystemStatus>> status();
  Future<Either<Failure, List<RegisteredPhone>>> phones();

  /// 404 = 이미 없음
  Future<Either<Failure, Unit>> revokePhone(String phoneId);

  /// 409 VOICE_ACTIVE = 대화 중
  Future<Either<Failure, Unit>> reboot();

  /// 스피커에서 키 지우기 (LAN, 비밀값이 오가지 않으므로 허용)
  Future<Either<Failure, Unit>> deleteSpeakerKey(String key);

  /// 스피커 키 바꾸기. 비밀값은 BLE로만 보낸다 (D6). 사용자가 Key2를 3초 눌러 BLE 창을 연 뒤에 부른다
  Future<Either<Failure, Unit>> sendKeyOverBle(String key, String value);
}
