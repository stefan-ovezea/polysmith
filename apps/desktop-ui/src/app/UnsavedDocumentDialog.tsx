import { useTranslation } from "react-i18next";
import type { PendingUnsavedAction } from "./appState";

interface UnsavedDocumentDialogProps {
  action: PendingUnsavedAction;
  currentDocumentName: string;
  onCancel: () => void;
  onDiscard: () => void;
  onSave: () => void;
}

export function UnsavedDocumentDialog({
  action,
  currentDocumentName,
  onCancel,
  onDiscard,
  onSave,
}: UnsavedDocumentDialogProps) {
  const { t } = useTranslation();

  return (
    <div className="cad-modal-backdrop" role="presentation">
      <section
        className="cad-unsaved-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="cad-unsaved-dialog-title"
      >
        <div>
          <h2
            id="cad-unsaved-dialog-title"
            className="text-base font-semibold text-on-surface"
          >
            {t("unsavedDialog.title", { name: currentDocumentName })}
          </h2>
          <p className="mt-2 text-sm text-on-surface-muted">
            {t("unsavedDialog.body")}
          </p>
        </div>
        <div className="mt-5 flex flex-wrap justify-end gap-2">
          <button type="button" className="cad-ribbon-action" onClick={onCancel}>
            {t("common.cancel")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action"
            onClick={onDiscard}
          >
            {action.kind === "quit"
              ? t("unsavedDialog.quitWithoutSaving")
              : t("unsavedDialog.continueWithoutSaving")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary"
            onClick={onSave}
          >
            {t("unsavedDialog.saveFirst")}
          </button>
        </div>
      </section>
    </div>
  );
}
