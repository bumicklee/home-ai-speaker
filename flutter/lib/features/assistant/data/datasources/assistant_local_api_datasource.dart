import '../../../../core/constants/api_paths.dart';
import '../../../../core/device/current_device.dart';

/// 비서 LAN API (펌웨어 `local_api.c` get/put_settings, get_voices, get_memories, delete_memory)
class AssistantLocalApiDataSource {
  AssistantLocalApiDataSource(this._device);

  final CurrentDevice _device;

  Future<Map<String, dynamic>> settings() async =>
      Map<String, dynamic>.from(await _device.client.get<Map<String, dynamic>>(ApiPaths.settings(SettingsGroup.assistant)));

  /// 성공하면 기기가 저장한 전체 설정을 돌려준다. 틀린 값은 400 INVALID_ARG
  Future<Map<String, dynamic>> update(Map<String, dynamic> changes) async => Map<String, dynamic>.from(
      await _device.client.put<Map<String, dynamic>>(ApiPaths.settings(SettingsGroup.assistant), changes));

  /// `{"voices": ["alloy", …]}`
  Future<List<String>> voices() async {
    final j = await _device.client.get<Map<String, dynamic>>(ApiPaths.voices);
    return [for (final v in j['voices'] as List<dynamic>? ?? const []) v as String];
  }

  /// `{"키": "값", …}` (최대 12개)
  Future<Map<String, String>> memories() async {
    final j = await _device.client.get<Map<String, dynamic>>(ApiPaths.memories);
    return {for (final e in j.entries) e.key: e.value as String};
  }

  /// 없는 키를 지워도 성공. 키 형식이 틀리면 400 INVALID_ARG
  Future<void> forget(String key) => _device.client.delete<dynamic>(ApiPaths.memory(key));
}
