import { useTranslation } from "react-i18next";
import type { SketchDeleteSelection } from "./appState";
import type { PendingSketchDeleteConfirmation } from "./deleteConfirmations";

interface SketchDeleteConfirmationPanelProps {
  confirmation: PendingSketchDeleteConfirmation;
  onConfirm: (selection: SketchDeleteSelection) => void;
  onCancel: () => void;
}

export function SketchDeleteConfirmationPanel({
  confirmation,
  onConfirm,
  onCancel,
}: SketchDeleteConfirmationPanelProps) {
  const { t } = useTranslation();
  const affectedCount = confirmation.affectedFeatureNames.length;

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="flex items-start gap-3">
        <span className="mt-1 flex h-9 w-9 shrink-0 items-center justify-center rounded-md bg-amber-400/15 text-amber-300 ring-1 ring-amber-300/35">
          <svg
            viewBox="0 0 16 16"
            width="20"
            height="20"
            aria-hidden="true"
            fill="none"
            stroke="currentColor"
            strokeWidth="1.5"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M8 2 14 13H2Z" />
            <path d="M8 6v3" />
            <path d="M8 11.5h.01" />
          </svg>
        </span>
        <div>
          <p className="cad-kicker text-amber-300">
            {t("sketchDelete.warning")}
          </p>
          <h2 className="mt-2 font-display text-lg text-on-surface">
            {t("sketchDelete.title")}
          </h2>
          <p className="mt-3 text-sm leading-5 text-on-surface-muted">
            {t("sketchDelete.body", {
              count: affectedCount,
              plural: affectedCount === 1 ? "" : "s",
              names: confirmation.affectedFeatureNames.join(", "),
            })}
          </p>
        </div>
      </div>
      <div className="mt-5 grid grid-cols-2 gap-3">
        <button
          type="button"
          className="rounded-md bg-primary px-4 py-2 text-sm font-medium text-on-primary transition-opacity hover:opacity-90"
          onClick={() => onConfirm(confirmation.selection)}
        >
          {t("common.ok")}
        </button>
        <button
          type="button"
          className="rounded-md bg-white/8 px-4 py-2 text-sm text-on-surface transition-colors hover:bg-white/12"
          onClick={onCancel}
        >
          {t("common.cancel")}
        </button>
      </div>
    </section>
  );
}
