import { useTranslation } from "react-i18next";

interface PreviewPanelActionsProps {
  confirmDisabled: boolean;
  cancelDisabled: boolean;
  onCancel: () => void;
}

export function PreviewPanelActions({
  confirmDisabled,
  cancelDisabled,
  onCancel,
}: PreviewPanelActionsProps) {
  const { t } = useTranslation();

  return (
    <div className="flex gap-3">
      <button
        type="submit"
        className="cad-action-primary flex-1"
        disabled={confirmDisabled}
      >
        {t("common.confirm")}
      </button>
      <button
        type="button"
        className="cad-action-ghost flex-1"
        disabled={cancelDisabled}
        onClick={onCancel}
      >
        {t("common.cancel")}
      </button>
    </div>
  );
}
