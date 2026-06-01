import { useTranslation } from "react-i18next";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";
const ICON_BUTTON_DISABLED = "cad-icon-button cad-icon-tool h-9 w-9 p-0 opacity-40";

export interface CamCuttingToolbarProps {
  disabled: boolean;
}

export function CamCuttingToolbar({ disabled }: CamCuttingToolbarProps) {
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
        data-tooltip={t("cam.cutting.twoD")} aria-label={t("cam.cutting.twoD")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="4" width="18" height="16" rx="1" />
          <path d="M3 12h18" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.cutting.nest")} aria-label={t("cam.cutting.nest")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="3" width="8" height="8" rx="1" />
          <rect x="13" y="3" width="8" height="8" rx="1" />
          <rect x="3" y="13" width="8" height="8" rx="1" />
          <rect x="13" y="13" width="8" height="8" rx="1" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.cutting.leadIn")} aria-label={t("cam.cutting.leadIn")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <path d="M18 6 6 18" />
          <circle cx="6" cy="6" r="2" />
          <circle cx="18" cy="18" r="2" />
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
