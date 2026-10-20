import { useTranslation } from "react-i18next";
import { createPortal } from "react-dom";

import type { FeatureEntry, PcbLayerParameters, PcbRole } from "@/types";
import { Dropdown } from "@/lib";
import { PCB_ROLE_OPTIONS } from "@/app/pcbLayerDefaults";
import { useContextMenuDismiss } from "./hooks/useContextMenuDismiss";

// Properties for an imported fabrication layer, opened from the hierarchy's
// right-click menu.  This is what the layer table used to show — and the reason
// the table existed: a role is either read from the file or guessed from its
// name, a mask is plotted NEGATIVE, a layer can be plotted mirrored, and small
// features get skipped.  Those are facts about the imported file, so they
// belong on the file.
//
// A fixed floating card rather than a modal: the viewport stays usable while
// it is open, so "Mirrored" can be toggled and watched.  Portaled to
// document.body for the same reason the context menus are — `.cad-sidebar` sets
// `backdrop-filter`, which makes it a containing block for fixed descendants
// and would otherwise leave the card behind an overlay that swallowed its
// clicks.  It dismisses like a menu (Escape, or a click anywhere else), because
// a panel with no way out is worse than no panel.

interface PcbLayerPropertiesPanelProps {
  layer: FeatureEntry;
  onClose: () => void;
  onUpdate: (
    featureId: string,
    options: { role?: string; mirrorX?: boolean },
  ) => Promise<void>;
  onRemove: (featureId: string) => Promise<void>;
}

function isDrill(params: PcbLayerParameters): boolean {
  return params.format === "excellon";
}

export function PcbLayerPropertiesPanel({
  layer,
  onClose,
  onUpdate,
  onRemove,
}: PcbLayerPropertiesPanelProps) {
  const { t } = useTranslation();
  const params = layer.pcb_layer_parameters;
  // Hook before any early return: an outside click or Escape closes the card.
  useContextMenuDismiss(Boolean(params), onClose);
  if (!params) {
    return null;
  }

  const width = params.max_x - params.min_x;
  const height = params.max_y - params.min_y;
  const drill = isDrill(params);

  const row = (label: string, value: React.ReactNode) => (
    <div className="flex items-baseline justify-between gap-4 py-1">
      <span className="text-[10px] uppercase tracking-[0.18em] text-on-surface-muted">
        {label}
      </span>
      <span className="text-right text-xs text-on-surface">{value}</span>
    </div>
  );

  return createPortal(
    <div
      className="cad-context-menu fixed right-6 top-24 z-40 w-[320px] rounded-xl p-3 shadow-[0_8px_24px_rgba(0,0,0,0.5)] backdrop-blur-xl"
      // Clicks inside the card must not reach the viewport behind it: the
      // sketch picking listens at the window level, so a click that bubbles
      // would select geometry through the card.  stopPropagation on the
      // container (not just the buttons) is what keeps the card modal-ish.
      onClick={(event) => event.stopPropagation()}
      onPointerDown={(event) => event.stopPropagation()}
      onPointerUp={(event) => event.stopPropagation()}
      onMouseDown={(event) => event.stopPropagation()}
      onMouseUp={(event) => event.stopPropagation()}
      onContextMenu={(event) => {
        event.preventDefault();
        event.stopPropagation();
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="min-w-0">
          <p className="truncate text-sm font-semibold text-on-surface">
            {layer.name}
          </p>
          <p className="truncate text-[10px] text-on-surface-muted" title={params.file_path}>
            {params.file_path}
          </p>
        </div>
        <button
          type="button"
          className="cad-action-ghost h-6 shrink-0 px-2"
          onClick={onClose}
        >
          {t("common.close")}
        </button>
      </div>

      <div className="mt-3 divide-y divide-outline/40">
        <div className="py-1">
          <label className="block text-[10px] uppercase tracking-[0.18em] text-on-surface-muted">
            {t("pcb.columnRole")}
            <Dropdown
              className="mt-1 w-full"
              value={params.role}
              label={t("pcb.columnRole")}
              options={PCB_ROLE_OPTIONS.map((role: PcbRole) => ({
                value: role,
                label: t(`pcb.role.${role}`),
              }))}
              onChange={(value) => {
                void onUpdate(layer.feature_id, { role: value });
              }}
            />
          </label>
          <p className="mt-1 text-[10px] text-on-surface-muted">
            {params.role_from_attributes
              ? t("pcb.roleFromFile", { function: params.file_function })
              : t("pcb.roleGuessed")}
          </p>
        </div>

        {row(
          t("pcb.columnContents"),
          drill
            ? t("pcb.contentsDrills", {
                tools: params.tool_count,
                holes: params.hit_count,
              })
            : t("pcb.contentsRings", { count: params.ring_count }),
        )}
        {row(
          t("pcb.columnPolarity"),
          params.file_polarity ||
            t("pcb.polarityAssumed", "Positive (assumed)"),
        )}
        {params.bounds_valid
          ? row(
              t("common.size", "Size"),
              t("pcb.bounds", {
                width: width.toFixed(1),
                height: height.toFixed(1),
              }),
            )
          : null}

        <div className="py-1">
          <label className="flex items-center justify-between gap-4">
            <span className="text-[10px] uppercase tracking-[0.18em] text-on-surface-muted">
              {t("pcb.columnMirror")}
            </span>
            <span className="flex items-center gap-2 text-xs text-on-surface">
              {params.mirror_x ? t("pcb.mirrored") : t("pcb.notMirrored")}
              <input
                type="checkbox"
                checked={params.mirror_x}
                onChange={(event) => {
                  void onUpdate(layer.feature_id, {
                    mirrorX: event.target.checked,
                  });
                }}
              />
            </span>
          </label>
        </div>

        {params.warnings.length > 0 ? (
          <div className="py-1">
            <p className="text-[10px] uppercase tracking-[0.18em] text-on-surface-muted">
              {t("pcb.warningCount", { count: params.warnings.length })}
            </p>
            <ul className="mt-1 space-y-1">
              {params.warnings.map((warning, index) => (
                <li
                  key={`${index}-${warning}`}
                  className="text-[10px] leading-relaxed text-on-surface-dim"
                >
                  {warning}
                </li>
              ))}
            </ul>
          </div>
        ) : null}
      </div>

      <div className="mt-3 flex justify-end">
        <button
          type="button"
          className="cad-action-ghost h-7"
          onClick={() => {
            void onRemove(layer.feature_id).then(onClose);
          }}
        >
          {t("pcb.remove")}
        </button>
      </div>
    </div>,
    document.body,
  );
}
