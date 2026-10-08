/// 키 입력 → "Key2를 3초 누르세요" → BLE로 보내기 (시스템 > 키, 비서 > AI · 대화 방식이 같이 쓴다).
/// 비밀값은 BLE로만 보낸다 (D6). 보내는 일은 [onSend]가 한다.
library;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../errors/error_localizer.dart';
import '../errors/failures.dart';
import '../../l10n/app_localizations.dart';
import 'app_widgets.dart';

/// 보냈으면 true
Future<bool> showSendKeySheet(
  BuildContext context, {
  required String label,
  required String initial,
  required RegExp pattern,
  required String invalidText,
  required Future<Failure?> Function(String value) onSend,
}) async {
  final sent = await showModalBottomSheet<bool>(
    context: context,
    isScrollControlled: true,
    showDragHandle: true,
    builder: (_) =>
        SendKeySheet(label: label, initial: initial, pattern: pattern, invalidText: invalidText, onSend: onSend),
  );
  return sent == true;
}

class SendKeySheet extends StatefulWidget {
  const SendKeySheet({
    super.key,
    required this.label,
    required this.initial,
    required this.pattern,
    required this.invalidText,
    required this.onSend,
  });

  final String label;

  /// 휴대폰에 기억해 둔 값
  final String initial;
  final RegExp pattern;
  final String invalidText;

  /// 실패하면 Failure (시트에 보인다)
  final Future<Failure?> Function(String value) onSend;

  @override
  State<SendKeySheet> createState() => _SendKeySheetState();
}

class _SendKeySheetState extends State<SendKeySheet> {
  late final _field = TextEditingController(text: widget.initial);
  String? _error;
  bool _sending = false;

  @override
  void dispose() {
    _field.dispose();
    super.dispose();
  }

  Future<void> _paste() async {
    final data = await Clipboard.getData(Clipboard.kTextPlain);
    final text = data?.text?.trim();
    if (text != null && text.isNotEmpty) setState(() => _field.text = text);
  }

  Future<void> _submit() async {
    final l = AppLocalizations.of(context);
    final value = _field.text.trim();
    if (!widget.pattern.hasMatch(value)) {
      setState(() => _error = widget.invalidText);
      return;
    }
    setState(() {
      _error = null;
      _sending = true;
    });
    final f = await widget.onSend(value);
    if (!mounted) return;
    if (f == null) {
      Navigator.pop(context, true);
    } else {
      setState(() {
        _sending = false;
        _error = l.errorText(f);
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return Padding(
      padding: EdgeInsets.fromLTRB(16, 0, 16, 16 + MediaQuery.viewInsetsOf(context).bottom),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(widget.label, style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 12),
          TextField(
            controller: _field,
            obscureText: true,
            enabled: !_sending,
            autocorrect: false,
            enableSuggestions: false,
            decoration: InputDecoration(
              labelText: widget.label,
              errorText: _error,
              errorMaxLines: 3,
              suffixIcon: TextButton(onPressed: _sending ? null : _paste, child: Text(l.sysPaste)),
            ),
          ),
          const SizedBox(height: 12),
          NoticeBox(icon: Icons.bluetooth_rounded, child: Text(l.bleKey2Hint)),
          const SizedBox(height: 16),
          FilledButton(
            onPressed: _sending ? null : _submit,
            style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(52)),
            child: _sending
                ? Row(mainAxisSize: MainAxisSize.min, children: [
                    const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2)),
                    const SizedBox(width: 12),
                    Text(l.sysKeySending),
                  ])
                : Text(l.sysKeySend),
          ),
        ],
      ),
    );
  }
}
