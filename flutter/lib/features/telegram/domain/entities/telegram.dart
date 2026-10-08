import 'package:equatable/equatable.dart';

/// 스피커의 텔레그램 연결 상태 (펌웨어 `telegram_state_name`)
class TelegramState {
  TelegramState._();
  static const off = 'off'; // 토큰 없음
  static const connecting = 'connecting';
  static const ok = 'ok';
  static const conflict = 'conflict'; // 다른 곳에서 같은 봇을 폴링하거나 Webhook 설정 (TLG-13)
  static const tokenInvalid = 'token_invalid';
}

/// 등록된 대화방 하나 (`chats[]`: chat_id, alias, title, type)
class TelegramChat extends Equatable {
  const TelegramChat({required this.chatId, required this.alias, this.title = '', this.type = 'private'});

  final int chatId;

  /// 음성으로 부르는 이름 (“엄마한테 보내 줘”). 최대 32바이트
  final String alias;

  /// 텔레그램 대화방 이름 (개인 대화면 사람 이름)
  final String title;

  /// private / group / supergroup / channel
  final String type;

  bool get isGroup => type == 'group' || type == 'supergroup';

  TelegramChat copyWith({String? alias}) =>
      TelegramChat(chatId: chatId, alias: alias ?? this.alias, title: title, type: type);

  @override
  List<Object?> get props => [chatId, alias, title, type];
}

/// `GET /telegram/chats` 전체 + 토큰 유무
class TelegramOverview extends Equatable {
  const TelegramOverview({
    required this.state,
    this.bot = '',
    this.unread = 0,
    this.chats = const [],
    this.hasToken = true,
  });

  final String state;

  /// 봇 사용자 이름 (@ 없이). 아직 연결 전이면 빈 문자열
  final String bot;
  final int unread;
  final List<TelegramChat> chats;

  /// 스피커에 봇 토큰이 있는지. state가 off일 때만 /status로 다시 확인한다
  final bool hasToken;

  TelegramOverview copyWith({String? state, String? bot, int? unread, List<TelegramChat>? chats, bool? hasToken}) =>
      TelegramOverview(
        state: state ?? this.state,
        bot: bot ?? this.bot,
        unread: unread ?? this.unread,
        chats: chats ?? this.chats,
        hasToken: hasToken ?? this.hasToken,
      );

  @override
  List<Object?> get props => [state, bot, unread, chats, hasToken];
}

/// 메시지 알림 옵션 (`auto_read`, `quiet_start`, `quiet_end`; 시는 0–23)
class TelegramOptions extends Equatable {
  const TelegramOptions({this.autoRead = false, this.quietStart = 22, this.quietEnd = 7});

  final bool autoRead;
  final int quietStart;
  final int quietEnd;

  TelegramOptions copyWith({bool? autoRead, int? quietStart, int? quietEnd}) => TelegramOptions(
        autoRead: autoRead ?? this.autoRead,
        quietStart: quietStart ?? this.quietStart,
        quietEnd: quietEnd ?? this.quietEnd,
      );

  @override
  List<Object?> get props => [autoRead, quietStart, quietEnd];
}

/// `POST /telegram/pair` 응답: 6자리 코드 (1회용, 5번 틀리면 무효)
class PairingCode extends Equatable {
  const PairingCode({required this.code, required this.expiresIn, this.bot = ''});

  final String code;
  final Duration expiresIn;

  /// 그룹에서는 `/pair@봇이름 코드`로 보낸다
  final String bot;

  @override
  List<Object?> get props => [code, expiresIn, bot];
}

/// `tg_paired` 이벤트: 새로 등록된 대화방
class PairedChat extends Equatable {
  const PairedChat({required this.chatId, this.title = '', this.type = 'private'});

  final int chatId;
  final String title;
  final String type;

  @override
  List<Object?> get props => [chatId, title, type];
}
