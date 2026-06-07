import {
  CamFaceOpButton,
  CamIconButton,
  CamSetupButton,
  CamToolbarDivider,
} from "./CamToolbarControls";

export interface CamTurningToolbarProps {
  disabled: boolean;
}

export function CamTurningToolbar({ disabled }: CamTurningToolbarProps) {
  return (
    <div className="flex items-center gap-1.5">
      <CamSetupButton disabled={disabled} />

      <CamToolbarDivider />

      <CamIconButton labelKey="cam.turning.rough" inactive>
        <rect x="3" y="6" width="18" height="12" rx="2" />
        <path d="M7 6V4h10v2" />
      </CamIconButton>

      <CamIconButton labelKey="cam.turning.finish" inactive>
        <circle cx="12" cy="12" r="8" />
        <circle cx="12" cy="12" r="4" />
      </CamIconButton>

      <CamIconButton labelKey="cam.turning.groove" inactive>
        <rect x="3" y="8" width="18" height="8" rx="1" />
        <path d="M7 8V6M17 8V6M7 16v2M17 16v2" />
      </CamIconButton>

      <CamIconButton labelKey="cam.turning.thread" inactive>
        <path d="M4 6h16M4 12h16M4 18h16" />
      </CamIconButton>

      <CamToolbarDivider />

      <CamFaceOpButton />
    </div>
  );
}
