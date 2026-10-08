import 'package:dartz/dartz.dart';

import '../../../../core/errors/failures.dart';
import '../entities/telegram.dart';

/// 텔레그램 (설계서 2.11, 3.5절). 토큰은 BLE로만 스피커에 보낸다 (D6). 앱은 getUpdates를 부르지 않는다
abstract interface class TelegramRepository {
  /// 대화방·상태·봇 이름·읽지 않은 수
  Future<Either<Failure, TelegramOverview>> overview();

  /// 별칭 바꾸기 (비울 수 없고 32바이트 이하)
  Future<Either<Failure, Unit>> rename(int chatId, String alias);
  Future<Either<Failure, Unit>> remove(int chatId);

  Future<Either<Failure, TelegramOptions>> options();

  /// 바꾼 값만 보낸다. 스피커가 저장한 전체 값을 돌려준다
  Future<Either<Failure, TelegramOptions>> saveOptions(TelegramOptions options);

  /// 6자리 등록 코드. 토큰이 없으면 DeviceFailure 409 (`NO_TOKEN`)
  Future<Either<Failure, PairingCode>> startPairing();

  /// Telegram `getMe`로 토큰 확인. 맞으면 봇 사용자 이름, Telegram이 거절하면 null
  Future<Either<Failure, String?>> checkToken(String token);

  /// BLE로 스피커에 토큰 저장 (사용자가 Key2를 3초 눌러 BLE 창을 연 뒤)
  Future<Either<Failure, Unit>> saveTokenToSpeaker(String token);

  /// 새로 등록된 대화방 (WS `tg_paired`)
  Stream<PairedChat> paired();

  /// 연결 상태 (WS `tg_status`)
  Stream<String> statusChanges();

  /// 읽지 않은 수 (WS `tg_message`)
  Stream<int> unreadChanges();
}
