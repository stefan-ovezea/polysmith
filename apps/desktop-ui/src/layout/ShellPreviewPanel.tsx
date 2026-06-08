import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface ShellPreviewPanelProps {
  isPending: boolean;
  initialThickness: number;
  faceSummary: string;
  disabled: boolean;
  onPreviewThickness: (thickness: number) => Promise<void> | void;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void> | void;
}

export function ShellPreviewPanel({
  isPending,
  initialThickness,
  faceSummary,
  disabled,
  onPreviewThickness,
  onConfirm,
  onCancel,
}: ShellPreviewPanelProps) {
  const { t } = useTranslation();

  async function handleConfirm() {
    await onConfirm();
  }

  return (
    <NumericPreviewPanel
      title={t("panels.shell.title")}
      helperText={
        isPending
          ? t("panels.shell.pickFace")
          : faceSummary
            ? t("panels.shell.fromFace", { face: faceSummary })
            : t("panels.shell.adjustThickness")
      }
      valueLabel={t("forms.thicknessMm")}
      initialValue={initialThickness}
      disabled={disabled}
      canConfirm={!isPending}
      inputMin="0.1"
      inputStep="0.1"
      onPreviewValue={onPreviewThickness}
      onConfirm={handleConfirm}
      onCancel={onCancel}
    />
  );
}
