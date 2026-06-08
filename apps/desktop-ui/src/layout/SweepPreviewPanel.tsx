import { useTranslation } from "react-i18next";
import { useConfirmCancelHotkeys } from "./hooks/useConfirmCancelHotkeys";

interface SweepPreviewPanelProps {
  phase: "pending" | "active";
  profileLabel: string | null;
  pathLabel: string | null;
  disabled: boolean;
  canConfirm: boolean;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void>;
}

export function SweepPreviewPanel({
  profileLabel,
  pathLabel,
  disabled,
  canConfirm,
  onConfirm,
  onCancel,
}: SweepPreviewPanelProps) {
  const { t } = useTranslation();

  useConfirmCancelHotkeys({ canConfirm, disabled, onCancel, onConfirm });

  return (
    <section className="pointer-events-auto cad-floating-panel box-border w-80 max-w-[calc(100vw-2rem)] overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("panels.sweep.title")}</p>
      <div className="mt-3 space-y-2 rounded-md bg-surface-container-low px-3 py-2 text-xs uppercase tracking-[0.16em] text-on-surface-muted">
        <div className="flex items-center justify-between gap-3">
          <span>{t("panels.sweep.profile")}</span>
          <span className="min-w-0 truncate text-on-surface">
            {profileLabel ?? t("panels.sweep.pickProfile")}
          </span>
        </div>
        <div className="flex items-center justify-between gap-3">
          <span>{t("panels.sweep.path")}</span>
          <span className="min-w-0 truncate text-on-surface">
            {pathLabel ?? t("panels.sweep.pickPath")}
          </span>
        </div>
      </div>
      <div className="mt-4 flex gap-3">
        <button
          type="button"
          className="cad-action-primary flex-1"
          disabled={disabled || !canConfirm}
          onClick={() => {
            void onConfirm();
          }}
        >
          {t("common.confirm")}
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
      <p className="mt-4 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
        {t("panels.sweep.pickHint")}
      </p>
    </section>
  );
}
