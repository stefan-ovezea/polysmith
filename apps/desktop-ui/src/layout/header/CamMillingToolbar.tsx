import { useTranslation } from "react-i18next";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";
const ICON_BUTTON_DISABLED = "cad-icon-button cad-icon-tool h-9 w-9 p-0 opacity-40";

export interface CamMillingToolbarProps {
  disabled: boolean;
  hasSetup: boolean;
  onSetupClick: () => void;
  onFaceMillingClick: () => void;
}

export function CamMillingToolbar({
  disabled,
  hasSetup,
  onSetupClick,
  onFaceMillingClick,
}: CamMillingToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-1.5">
      {/* Common setup — shared across categories */}
      <button type="button"
        className={hasSetup && !disabled ? ICON_BUTTON_DISABLED : ICON_BUTTON_BASE}
        data-tooltip={hasSetup ? t("cam.common.setupDone") : t("cam.common.setup")}
        aria-label={hasSetup ? t("cam.common.setupDone") : t("cam.common.setup")}
        disabled={hasSetup || disabled}
        onClick={onSetupClick}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <circle cx="12" cy="12" r="3" />
          <path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42" />
        </svg>
      </button>

      <div className="w-px h-6 cad-panel-soft-border mx-1" />

      <button type="button" className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.profile")} aria-label={t("cam.profile")} disabled={disabled}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="4" width="18" height="16" rx="2" />
          <rect x="7" y="8" width="10" height="8" rx="1" />
          <circle cx="12" cy="12" r="2" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.pocket")} aria-label={t("cam.pocket")} disabled={disabled}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="4" width="18" height="16" rx="2" />
          <path d="M7 8h10M7 12h10M7 16h6" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.drill")} aria-label={t("cam.drill")} disabled={disabled}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <circle cx="12" cy="12" r="3" />
          <circle cx="12" cy="12" r="1" fill="currentColor" />
          <path d="M12 15v4M10 19h4" />
        </svg>
      </button>

      <div className="w-px h-6 cad-panel-soft-border mx-1" />

      <button type="button"
        className={hasSetup && !disabled ? ICON_BUTTON_BASE : ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.common.faceOp")}
        aria-label={t("cam.common.faceOp")}
        disabled={!hasSetup || disabled}
        onClick={onFaceMillingClick}>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <rect x="3" y="3" width="18" height="18" rx="2" />
          <path d="M9 3v18" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.common.contour")} aria-label={t("cam.common.contour")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <path d="M4 4h16v16H4z" />
          <path d="M8 8h8v8H8z" />
        </svg>
      </button>

      <button type="button" className={ICON_BUTTON_DISABLED}
        data-tooltip={t("cam.common.engrave")} aria-label={t("cam.common.engrave")} disabled>
        <svg viewBox="0 0 24 24" className="h-7 w-7" fill="none"
          stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"
          strokeLinejoin="round" aria-hidden="true">
          <path d="M4 20h16" />
          <path d="M8 20V8a4 4 0 018 0v12" />
        </svg>
      </button>
    </div>
  );
}
