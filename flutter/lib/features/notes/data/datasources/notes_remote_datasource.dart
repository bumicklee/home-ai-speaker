import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';

/// 선택된 스피커의 메모·일정 LAN API (설계서 docs/design-notes-schedule.md 5.3절). 예외는 DeviceApiClient가 AppException으로 바꿔 던진다
class NotesRemoteDataSource {
  NotesRemoteDataSource(this._device);

  final CurrentDevice _device;

  /// `{rev, items}`: [since] 이후 바뀐 항목 (지운 기록 포함). 0이면 전체
  Future<Map<String, dynamic>> changes(int since) =>
      _device.client.get<Map<String, dynamic>>(ApiPaths.notesSince(since));

  /// 만든 항목. 400 INVALID_ARG, 409 FULL
  Future<Map<String, dynamic>> create(Map<String, dynamic> body) =>
      _device.client.post<Map<String, dynamic>>(ApiPaths.notes, body);

  /// 고친 항목. 404 NOT_FOUND, 409 CONFLICT (본문 `note` = 최신)
  Future<Map<String, dynamic>> update(String id, Map<String, dynamic> body) =>
      _device.client.put<Map<String, dynamic>>(ApiPaths.note(id), body);

  /// 스피커 원본 삭제 → `{id, rev, state: "gone"}`
  Future<Map<String, dynamic>> delete(String id) => _device.client.delete<Map<String, dynamic>>(ApiPaths.note(id));

  /// 휴지통에서 되살린 항목
  Future<Map<String, dynamic>> restore(String id) =>
      _device.client.post<Map<String, dynamic>>(ApiPaths.noteRestore(id));

  /// `{enabled, default_remind, default_remind_all_day, all_day_time}`
  Future<Map<String, dynamic>> settings() => _device.client.get<Map<String, dynamic>>(ApiPaths.notesSettings);

  /// 일부만 보내도 된다. 저장 뒤 전체 값
  Future<Map<String, dynamic>> saveSettings(Map<String, dynamic> body) =>
      _device.client.put<Map<String, dynamic>>(ApiPaths.notesSettings, body);
}
