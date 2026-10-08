import 'package:flutter/material.dart';

/// 알림 칩 (고르면 청록으로 채움, 3개 찬 뒤 나머지는 흐리게, 직접 입력은 점선 대신 외곽선)
class RemindChip extends StatelessWidget {
  const RemindChip({super.key, required this.label, required this.selected, this.onTap, this.custom = false});

  final String label;
  final bool selected;

  /// null = 고를 수 없음
  final VoidCallback? onTap;
  final bool custom;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final enabled = onTap != null;
    final fg = selected
        ? scheme.onPrimary
        : !enabled
            ? scheme.onSurface.withValues(alpha: 0.38)
            : custom
                ? scheme.onSurfaceVariant
                : scheme.onSurface;
    return Semantics(
      button: true,
      selected: selected,
      enabled: enabled,
      child: Material(
        color: selected ? scheme.primary : scheme.surface,
        shape: StadiumBorder(
          side: BorderSide(
            color: selected ? scheme.primary : (custom ? scheme.onSurfaceVariant.withValues(alpha: 0.5) : scheme.outline),
            width: selected ? 1.5 : 1,
          ),
        ),
        child: InkWell(
          customBorder: const StadiumBorder(),
          onTap: onTap,
          child: ConstrainedBox(
            constraints: const BoxConstraints(minHeight: 40),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 14),
              child: Row(mainAxisSize: MainAxisSize.min, children: [
                if (custom && !selected) ...[Icon(Icons.add, size: 16, color: fg), const SizedBox(width: 4)],
                Text(label,
                    style: TextStyle(
                        fontSize: 14, color: fg, fontWeight: selected ? FontWeight.w700 : FontWeight.w500)),
              ]),
            ),
          ),
        ),
      ),
    );
  }
}

/// 둘·셋 중 하나 고르기 (디자인: 회색 바탕 위 흰 칸)
class SegmentTabs extends StatelessWidget {
  const SegmentTabs({super.key, required this.labels, required this.selected, required this.onChanged});

  final List<String> labels;
  final int selected;
  final ValueChanged<int> onChanged;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(color: scheme.surfaceContainerHighest, borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(4),
        child: Row(children: [
          for (var i = 0; i < labels.length; i++) ...[
            if (i > 0) const SizedBox(width: 4),
            Expanded(
              child: Semantics(
                selected: i == selected,
                button: true,
                child: Material(
                  color: i == selected ? scheme.surface : Colors.transparent,
                  elevation: i == selected ? 1 : 0,
                  borderRadius: BorderRadius.circular(9),
                  child: InkWell(
                    borderRadius: BorderRadius.circular(9),
                    onTap: () => onChanged(i),
                    child: SizedBox(
                      height: 40,
                      child: Center(
                        child: Text(labels[i],
                            style: TextStyle(
                              fontSize: 15,
                              color: i == selected ? scheme.primary : scheme.onSurfaceVariant,
                              fontWeight: i == selected ? FontWeight.w700 : FontWeight.w500,
                            )),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ]),
      ),
    );
  }
}

/// 둘 중 하나 (메모/일정): 고른 쪽은 청록 외곽선
class KindChoice extends StatelessWidget {
  const KindChoice({super.key, required this.labels, required this.selected, this.onChanged});

  final List<String> labels;
  final int selected;
  final ValueChanged<int>? onChanged;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Row(children: [
      for (var i = 0; i < labels.length; i++) ...[
        if (i > 0) const SizedBox(width: 8),
        Expanded(
          child: Semantics(
            selected: i == selected,
            inMutuallyExclusiveGroup: true,
            button: true,
            child: Material(
              color: i == selected ? scheme.primaryContainer : scheme.surface,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
                side: BorderSide(color: i == selected ? scheme.primary : scheme.outline, width: i == selected ? 1.5 : 1),
              ),
              child: InkWell(
                borderRadius: BorderRadius.circular(12),
                onTap: onChanged == null ? null : () => onChanged!(i),
                child: SizedBox(
                  height: 48,
                  child: Center(
                    child: Text(labels[i],
                        style: TextStyle(
                          fontSize: 16,
                          color: i == selected ? scheme.primary : scheme.onSurface,
                          fontWeight: i == selected ? FontWeight.w700 : FontWeight.w500,
                        )),
                  ),
                ),
              ),
            ),
          ),
        ),
      ],
    ]);
  }
}
