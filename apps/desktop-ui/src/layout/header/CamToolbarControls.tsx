import { useTranslation } from "react-i18next";
import type { ReactNode } from "react";

const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";
const ICON_BUTTON_DISABLED =
  "cad-icon-button cad-icon-tool h-9 w-9 p-0 opacity-40";
const ICON_SIZE_CLASS = "h-7 w-7";

interface CamIconButtonProps {
  labelKey: string;
  disabled?: boolean;
  inactive?: boolean;
  onClick?: () => void;
  children: ReactNode;
}

export function CamIconButton({
  labelKey,
  disabled = false,
  inactive = false,
  onClick,
  children,
}: CamIconButtonProps) {
  const { t } = useTranslation();
  const label = t(labelKey);

  return (
    <button
      type="button"
      className={inactive ? ICON_BUTTON_DISABLED : ICON_BUTTON_BASE}
      data-tooltip={label}
      aria-label={label}
      disabled={disabled || inactive}
      onClick={onClick}
    >
      <svg
        viewBox="0 0 24 24"
        className={ICON_SIZE_CLASS}
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        {children}
      </svg>
    </button>
  );
}

export function CamToolbarDivider() {
  return <div className="w-px h-6 cad-panel-soft-border mx-1" />;
}

export function CamSetupButton({ disabled }: { disabled: boolean }) {
  return (
    <CamIconButton labelKey="cam.common.setup" disabled={disabled}>
      <circle cx="12" cy="12" r="3" />
      <path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42" />
    </CamIconButton>
  );
}

export function CamFaceOpButton({
  disabled = false,
  onClick = () => {},
}: {
  disabled?: boolean;
  onClick?: () => void;
}) {
  return (
    <CamIconButton labelKey="cam.common.faceOp" disabled={disabled} onClick={onClick}>
      <rect x="3" y="3" width="18" height="18" rx="2" />
      <path d="M9 3v18" />
    </CamIconButton>
  );
}
