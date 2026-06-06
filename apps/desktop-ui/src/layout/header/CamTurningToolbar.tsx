import { useTranslation } from "react-i18next";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";
const ICON_BUTTON_DISABLED = "cad-icon-button cad-icon-tool h-9 w-9 p-0 opacity-40";

export interface CamTurningToolbarProps {
  disabled: boolean;
}

export function CamTurningToolbar({ disabled }: CamTurningToolbarProps) {
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
        data-tooltip={t("cam.turning.rough")} aria-label={t("cam.turning.rough")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="6" width="18" height="12" rx="2" />
          <path d="M7 6V4h10v2" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.turning.finish")} aria-label={t("cam.turning.finish")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <circle cx="12" cy="12" r="8" />
          <circle cx="12" cy="12" r="4" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.turning.groove")} aria-label={t("cam.turning.groove")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="8" width="18" height="8" rx="1" />
          <path d="M7 8V6M17 8V6M7 16v2M17 16v2" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.turning.thread")} aria-label={t("cam.turning.thread")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <path d="M4 6h16M4 12h16M4 18h16" />
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
