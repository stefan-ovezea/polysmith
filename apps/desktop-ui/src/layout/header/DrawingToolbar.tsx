import { useTranslation } from "react-i18next";

export interface DrawingToolbarProps {
  disabled: boolean;
}

/** Placeholder toolbar for the ISO Drawing workspace.
 *  Dimension tools and drawing-sheet controls will be added here. */
export function DrawingToolbar({ disabled }: DrawingToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-3">
      <span className="text-xs text-[var(--cad-muted)]">
        {t("drawing.placeholder")}
      </span>
    </div>
  );
}
