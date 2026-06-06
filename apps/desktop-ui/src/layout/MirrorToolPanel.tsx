import { useEffect } from "react";
import { useTranslation } from "react-i18next";

interface MirrorToolPanelProps {
  axisLineId: string | null;
  objectIds: string[];
  generatedLineCount: number;
  generatedCircleCount: number;
  focusedSlot: "objects" | "axis" | null;
  persistent: boolean;
  onTogglePersistent: () => void;
  disabled: boolean;
  onFocusObjects: () => void;
  onFocusAxis: () => void;
  onClearObjects: () => Promise<void>;
  onClearAxis: () => Promise<void>;
  onConfirm: () => Promise<void>;
  onCancel: () => Promise<void>;
}

// Floating contextual modeling panel for the in-progress Mirror tool.
// The native core has already opened a `pending_mirror` on the
// active sketch, and is regenerating preview geometry on every
// `update_mirror_preview_axis` / `update_mirror_preview_objects`
// call. The panel itself is a thin shell over those calls — it
// only owns the *focused slot* (which slot the next entity
// click should land in).
export function MirrorToolPanel({
  axisLineId,
  objectIds,
  generatedLineCount,
  generatedCircleCount,
  focusedSlot,
  persistent,
  onTogglePersistent,
  disabled,
  onFocusObjects,
  onFocusAxis,
  onClearObjects,
  onClearAxis,
  onConfirm,
  onCancel,
}: MirrorToolPanelProps) {
  const { t } = useTranslation();
  // Esc / Enter shortcuts when the panel has focus. Global hotkeys
  // (when focus is in the canvas) live in App.tsx — those take
  // precedence and call the same callbacks.
  useEffect(() => {
    function handleKey(event: KeyboardEvent) {
      if (event.key === "Escape") {
        event.preventDefault();
        void onCancel();
      } else if (event.key === "Enter") {
        event.preventDefault();
        void onConfirm();
      }
    }
    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, [onCancel, onConfirm]);

  const totalGenerated = generatedLineCount + generatedCircleCount;
  // Apply requires both slots to be filled. Mirroring with no
  // objects (or no axis) wouldn't produce geometry, so we gate
  // the button rather than letting the user commit a no-op.
  const canApply = axisLineId !== null && objectIds.length > 0 && !disabled;

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5 w-72">
      <p className="cad-kicker">{t("panels.mirror.title")}</p>
      <p className="mt-3 text-xs text-on-surface-muted">
        {t("panels.mirror.instructions")}
      </p>

      <div className="mt-4 space-y-3">
        {/* Objects slot. Clicking the slot focuses it; viewport
            entity clicks then add to the list (and clicking an
            already-included entity removes it — handled in App). */}
        <button
          type="button"
          className={
            focusedSlot === "objects"
              ? "cad-input cad-input-active w-full text-left"
              : "cad-input w-full text-left"
          }
          disabled={disabled}
          onClick={onFocusObjects}
        >
          <span className="block text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
            {t("panels.mirror.objects")}
          </span>
          <span className="mt-1 block text-sm">
            {objectIds.length === 0
              ? t("panels.mirror.clickToSelect")
              : t("panels.mirror.selected", { count: objectIds.length })}
          </span>
        </button>
        {objectIds.length > 0 ? (
          <button
            type="button"
            className="cad-link-button text-[11px] uppercase tracking-[0.16em]"
            disabled={disabled}
            onClick={() => {
              void onClearObjects();
            }}
          >
            {t("panels.mirror.clearObjects")}
          </button>
        ) : null}

        {/* Axis slot. Same focus pattern; viewport line clicks set
            the axis. Picking a circle is rejected by the core
            (it can't be a mirror axis). */}
        <button
          type="button"
          className={
            focusedSlot === "axis"
              ? "cad-input cad-input-active w-full text-left"
              : "cad-input w-full text-left"
          }
          disabled={disabled}
          onClick={onFocusAxis}
        >
          <span className="block text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
            {t("panels.mirror.axis")}
          </span>
          <span className="mt-1 block text-sm">
            {/* Never expose internal ids in the UI — the user
                only needs to know whether something is selected. */}
            {axisLineId
              ? t("panels.mirror.selected", { count: 1 })
              : t("panels.mirror.clickToSelect")}
          </span>
        </button>
        {axisLineId !== null ? (
          <button
            type="button"
            className="cad-link-button text-[11px] uppercase tracking-[0.16em]"
            disabled={disabled}
            onClick={() => {
              void onClearAxis();
            }}
          >
            {t("panels.mirror.clearAxis")}
          </button>
        ) : null}
      </div>

      <p className="mt-4 text-[11px] text-on-surface-dim">
        {t("panels.mirror.preview", {
          count: totalGenerated,
          entityLabel: totalGenerated === 1 ? "entity" : "entities",
        })}
      </p>

      <label className="mt-4 flex items-center gap-2 cursor-pointer text-xs text-on-surface">
        <input
          type="checkbox"
          checked={persistent}
          onChange={onTogglePersistent}
          disabled={disabled}
          className="cad-checkbox"
        />
        {t("panels.mirror.persistent")}
      </label>

      <div className="mt-4 flex gap-3">
        <button
          type="button"
          className="cad-action-primary flex-1"
          disabled={!canApply}
          onClick={() => {
            void onConfirm();
          }}
        >
          {t("common.apply")}
        </button>
        <button
          type="button"
          className="cad-action-ghost flex-1"
          disabled={disabled}
          onClick={() => {
            void onCancel();
          }}
        >
          {t("common.cancel")}
        </button>
      </div>
      <p className="mt-3 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
        {t("panels.shortcutHint.apply")}
      </p>
    </section>
  );
}
