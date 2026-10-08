import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/errors/failures.dart';
import '../../domain/entities/system_status.dart';
import '../../domain/repositories/system_repository.dart';

class SystemState extends Equatable {
  const SystemState({
    this.status,
    this.phones = const [],
    this.loading = false,
    this.failure,
    this.phonesFailure,
    this.busy = false,
  });

  final SystemStatus? status;
  final List<RegisteredPhone> phones;
  final bool loading;

  /// `/status` 불러오기 실패 (화면의 나머지 — 키, 앱 설정, 휴대폰에서 지우기 — 는 그대로 쓴다)
  final Failure? failure;
  final Failure? phonesFailure;

  /// 해지·재시작 중
  final bool busy;

  RegisteredPhone? get self => phones.where((p) => p.self).firstOrNull;

  SystemState copyWith({
    SystemStatus? Function()? status,
    List<RegisteredPhone>? phones,
    bool? loading,
    Failure? Function()? failure,
    Failure? Function()? phonesFailure,
    bool? busy,
  }) =>
      SystemState(
        status: status != null ? status() : this.status,
        phones: phones ?? this.phones,
        loading: loading ?? this.loading,
        failure: failure != null ? failure() : this.failure,
        phonesFailure: phonesFailure != null ? phonesFailure() : this.phonesFailure,
        busy: busy ?? this.busy,
      );

  @override
  List<Object?> get props => [status, phones, loading, failure, phonesFailure, busy];
}

/// 시스템 화면: 기기 정보, 등록된 휴대폰, 다시 시작.
/// 동작 메서드는 실패를 돌려준다 (null = 성공). 화면이 스낵바로 알린다
class SystemCubit extends Cubit<SystemState> {
  SystemCubit(this._repo) : super(const SystemState());

  final SystemRepository _repo;

  String? get host => _repo.host;
  String? get deviceId => _repo.deviceId;

  Future<void> load() async {
    emit(state.copyWith(loading: true));
    final results = await Future.wait([_repo.status(), _repo.phones()]);
    if (isClosed) return;
    final status = results[0].fold((f) => f, (s) => s);
    final phones = results[1].fold((f) => f, (p) => p);
    emit(state.copyWith(
      loading: false,
      status: status is SystemStatus ? () => status : null,
      failure: () => status is Failure ? status : null,
      phones: phones is List<RegisteredPhone> ? phones : null,
      phonesFailure: () => phones is Failure ? phones : null,
    ));
  }

  /// 다른 휴대폰 등록 해지. 이미 없으면(404) 목록에서만 뺀다
  Future<Failure?> revoke(RegisteredPhone phone) async {
    emit(state.copyWith(busy: true));
    final r = await _repo.revokePhone(phone.phoneId);
    if (isClosed) return null;
    final failure = r.fold((f) => f is DeviceFailure && f.status == 404 ? null : f, (_) => null);
    emit(state.copyWith(
      busy: false,
      phones: failure == null ? [for (final p in state.phones) if (p.phoneId != phone.phoneId) p] : null,
    ));
    return failure;
  }

  /// 대화 중이면 409 VOICE_ACTIVE (errorDeviceVoiceActive)
  Future<Failure?> reboot() async {
    emit(state.copyWith(busy: true));
    final r = await _repo.reboot();
    if (isClosed) return null;
    emit(state.copyWith(busy: false));
    return r.fold((f) => f, (_) => null);
  }

  /// 휴대폰에서 스피커를 지우기 전에, 연결되면 스피커 쪽의 이 휴대폰 등록도 해지한다 (실패해도 계속)
  Future<bool> revokeSelf() async {
    var self = state.self;
    if (self == null) {
      final phones = await _repo.phones();
      self = phones.fold((_) => null, (list) => list.where((p) => p.self).firstOrNull);
    }
    if (self == null) return false;
    final r = await _repo.revokePhone(self.phoneId);
    return r.isRight();
  }
}
