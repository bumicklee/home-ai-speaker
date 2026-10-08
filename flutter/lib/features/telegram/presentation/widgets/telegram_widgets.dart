import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../../../core/theme/app_theme.dart';
import '../../../../core/widgets/app_widgets.dart';
import '../../../../l10n/app_localizations.dart';
import '../../domain/entities/telegram.dart';

/// 디자인 "텔레그램 연결"의 단계 카드 (번호 원 + 제목). 진행 중인 단계는 청록 테두리
class TgStepCard extends StatelessWidget {
  const TgStepCard({
    super.key,
    required this.number,
    required this.title,
    required this.child,
    this.done = false,
    this.active = false,
  });

  final int number;
  final String title;
  final Widget child;
  final bool done;
  final bool active;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final badge = done ? AppColors.ok : (active ? scheme.primary : scheme.onSurfaceVariant);
    return AppCard(
      border: active ? BorderSide(color: scheme.primary, width: 1.5) : null,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(children: [
            Container(
              width: 26,
              height: 26,
              alignment: Alignment.center,
              decoration: BoxDecoration(color: badge, shape: BoxShape.circle),
              child: done
                  ? const Icon(Icons.check_rounded, size: 16, color: Colors.white)
                  : Text('$number',
                      style: const TextStyle(color: Colors.white, fontSize: 13, fontWeight: FontWeight.w700)),
            ),
            const SizedBox(width: 10),
            Expanded(child: Text(title, style: Theme.of(context).textTheme.titleMedium)),
          ]),
          const SizedBox(height: 10),
          child,
        ],
      ),
    );
  }
}

/// 아이콘 + 한 줄 결과 (확인됨, 기다리는 중 등)
class TgStatusLine extends StatelessWidget {
  const TgStatusLine({super.key, required this.text, this.icon, this.color, this.busy = false});

  final String text;
  final IconData? icon;
  final Color? color;
  final bool busy;

  @override
  Widget build(BuildContext context) {
    final c = color ?? Theme.of(context).colorScheme.onSurfaceVariant;
    return Row(children: [
      if (busy)
        SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2, color: c))
      else if (icon != null)
        Icon(icon, size: 18, color: c),
      const SizedBox(width: 8),
      Expanded(child: Text(text, style: TextStyle(fontSize: 14, color: c, fontWeight: FontWeight.w500))),
    ]);
  }
}

/// 대화방 첫 글자 동그라미 (개인: 청록, 그룹: 호박)
class TgChatAvatar extends StatelessWidget {
  const TgChatAvatar({super.key, required this.chat});

  final TelegramChat chat;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final group = chat.isGroup;
    final name = chat.alias.trim();
    return ExcludeSemantics(
      child: CircleAvatar(
        radius: 20,
        backgroundColor: group ? scheme.errorContainer : scheme.primaryContainer,
        foregroundColor: group ? scheme.onErrorContainer : scheme.onPrimaryContainer,
        child: Text(name.isEmpty ? '?' : name.characters.first, style: const TextStyle(fontWeight: FontWeight.w700)),
      ),
    );
  }
}

/// "개인 대화 · Kim Mira"
String tgChatSubtitle(AppLocalizations l, TelegramChat chat) {
  final kind = chat.isGroup ? l.tgChatGroup : (chat.type == 'channel' ? l.tgChatChannel : l.tgChatPrivate);
  return chat.title.isEmpty || chat.title == chat.alias ? kind : '$kind · ${chat.title}';
}

/// 22 → "22:00"
String tgHour(int h) => '${h.toString().padLeft(2, '0')}:00';

/// 텔레그램 링크 열기 (앱이 있으면 앱으로)
Future<void> openTelegramLink(BuildContext context, String url) async {
  final l = AppLocalizations.of(context);
  final ok = await launchUrl(Uri.parse(url), mode: LaunchMode.externalApplication).catchError((_) => false);
  if (!ok && context.mounted) showMessage(context, l.tgOpenFailed);
}

/// 펌웨어 별칭 한도 (TG_ALIAS_MAX, UTF-8 바이트)
const tgAliasMaxBytes = 32;

/// 별칭 바꾸기 대화상자. 바꾼 값 또는 null(취소)
Future<String?> showRenameDialog(BuildContext context, TelegramChat chat) =>
    showDialog<String>(context: context, builder: (_) => _RenameDialog(initial: chat.alias));

class _RenameDialog extends StatefulWidget {
  const _RenameDialog({required this.initial});

  final String initial;

  @override
  State<_RenameDialog> createState() => _RenameDialogState();
}

class _RenameDialogState extends State<_RenameDialog> {
  late final _controller = TextEditingController(text: widget.initial);
  final _form = GlobalKey<FormState>();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    if (_form.currentState?.validate() ?? false) Navigator.pop(context, _controller.text.trim());
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return AlertDialog(
      title: Text(l.tgRenameTitle),
      content: Form(
        key: _form,
        child: TextFormField(
          controller: _controller,
          autofocus: true,
          textInputAction: TextInputAction.done,
          decoration: InputDecoration(helperText: l.tgAliasHelp),
          validator: (v) {
            final s = (v ?? '').trim();
            if (s.isEmpty) return l.tgAliasEmpty;
            if (utf8.encode(s).length > tgAliasMaxBytes) return l.tgAliasTooLong;
            return null;
          },
          onFieldSubmitted: (_) => _submit(),
        ),
      ),
      actions: [
        TextButton(onPressed: () => Navigator.pop(context), child: Text(l.commonCancel)),
        TextButton(onPressed: _submit, child: Text(l.tgSave)),
      ],
    );
  }
}

/// 조용한 시간 고르기 (시작·끝, 0–23시). 고른 값 또는 null(취소)
Future<(int, int)?> showQuietHoursSheet(BuildContext context, TelegramOptions options) =>
    showModalBottomSheet<(int, int)>(
      context: context,
      isScrollControlled: true,
      builder: (_) => _QuietHoursSheet(start: options.quietStart, end: options.quietEnd),
    );

class _QuietHoursSheet extends StatefulWidget {
  const _QuietHoursSheet({required this.start, required this.end});

  final int start;
  final int end;

  @override
  State<_QuietHoursSheet> createState() => _QuietHoursSheetState();
}

class _QuietHoursSheetState extends State<_QuietHoursSheet> {
  late int _start = widget.start.clamp(0, 23);
  late int _end = widget.end.clamp(0, 23);

  Widget _hourField(String label, int value, ValueChanged<int> onChanged) => DropdownButtonFormField<int>(
        initialValue: value,
        decoration: InputDecoration(labelText: label),
        items: [for (var h = 0; h < 24; h++) DropdownMenuItem(value: h, child: Text(tgHour(h)))],
        onChanged: (v) => v == null ? null : onChanged(v),
      );

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(l.tgQuietHours, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 16),
            Row(children: [
              Expanded(child: _hourField(l.tgQuietStart, _start, (v) => setState(() => _start = v))),
              const Padding(padding: EdgeInsets.symmetric(horizontal: 12), child: Text('–')),
              Expanded(child: _hourField(l.tgQuietEnd, _end, (v) => setState(() => _end = v))),
            ]),
            const SizedBox(height: 20),
            FilledButton(onPressed: () => Navigator.pop(context, (_start, _end)), child: Text(l.tgSave)),
          ],
        ),
      ),
    );
  }
}
