import { useTranslation } from "react-i18next";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";

export type CamOperationType = "faceMilling" | "profile" | "pocket" | "drill";

export interface CamToolbarProps {
  disabled: boolean;
  activeOperation: CamOperationType | null;
  onSelectOperation: (op: CamOperationType) => void;
}

function ProfileIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      className="h-7 w-7"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <rect x="3" y="4" width="18" height="16" rx="2" />
      <rect x="7" y="8" width="10" height="8" rx="1" />
      <circle cx="12" cy="12" r="2" />
    </svg>
  );
}

function PocketIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      className="h-7 w-7"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <rect x="3" y="4" width="18" height="16" rx="2" />
      <path d="M7 8h10M7 12h10M7 16h6" />
    </svg>
  );
}

function DrillIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      className="h-7 w-7"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <circle cx="12" cy="12" r="3" />
      <circle cx="12" cy="12" r="1" fill="currentColor" />
      <path d="M12 15v4M10 19h4" />
    </svg>
  );
}

export function CamToolbar({
  disabled,
  activeOperation,
  onSelectOperation,
}: CamToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-1.5">
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.profile")}
        aria-label={t("cam.profile")}
        onClick={() => onSelectOperation("profile")}
        disabled={disabled}
      >
        <ProfileIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.pocket")}
        aria-label={t("cam.pocket")}
        onClick={() => onSelectOperation("pocket")}
        disabled={disabled}
      >
        <PocketIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("cam.drill")}
        aria-label={t("cam.drill")}
        onClick={() => onSelectOperation("drill")}
        disabled={disabled}
      >
        <DrillIcon />
      </button>
    </div>
  );
}
