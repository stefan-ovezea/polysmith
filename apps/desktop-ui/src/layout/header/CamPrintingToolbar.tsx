import { useTranslation } from "react-i18next";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";
const ICON_BUTTON_DISABLED = "cad-icon-button cad-icon-tool h-9 w-9 p-0 opacity-40";

export interface CamPrintingToolbarProps {
  disabled: boolean;
}

export function CamPrintingToolbar({ disabled }: CamPrintingToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-1.5">
      <button type="button" className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.common.setup")} aria-label={t("cam.common.setup")}
        disabled={disabled}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <circle cx="12" cy="12" r="3" />
          <path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42" />
        </svg>
      </button>

      <div className="w-px h-6 cad-panel-soft-border mx-1" />

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.printing.slice")} aria-label={t("cam.printing.slice")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="3" width="18" height="18" rx="2" />
          <path d="M3 9h18M3 15h18" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.printing.support")} aria-label={t("cam.printing.support")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <path d="M12 20V4M8 20l4-4 4 4" />
          <path d="M6 4h12" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.printing.infill")} aria-label={t("cam.printing.infill")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="3" width="18" height="18" rx="2" />
          <path d="M3 3l18 18M21 3L3 21" />
        </svg>
      </button>

      <div className="w-px h-6 cad-panel-soft-border mx-1" />

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.common.faceOp")} aria-label={t("cam.common.faceOp")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="3" width="18" height="18" rx="2" />
          <path d="M9 3v18" />
        </svg>
      </button>
    </div>
  );
}
