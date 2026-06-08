import { useTranslation } from "react-i18next";

interface ActivePanelActionsProps {
  disabled: boolean;
  onCancel: () => void;
  onConfirm: () => void;
}

export function ActivePanelActions({
  disabled,
  onCancel,
  onConfirm,
}: ActivePanelActionsProps) {
  const { t } = useTranslation();

  return (
    <div className="mt-5 flex gap-3">
      <button
        type="button"
        className="cad-ribbon-action cad-ribbon-action-primary flex-1"
        disabled={disabled}
        onClick={onConfirm}
      >
        {t("common.confirm")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action flex-1"
        onClick={onCancel}
      >
        {t("common.cancel")}
      </button>
    </div>
  );
}
