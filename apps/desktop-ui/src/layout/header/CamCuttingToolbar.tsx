import {
  CamFaceOpButton,
  CamIconButton,
  CamSetupButton,
  CamToolbarDivider,
} from "./CamToolbarControls";

export interface CamCuttingToolbarProps {
  disabled: boolean;
  onTwoDCut: () => void;
  onFaceOpClick: () => void;
}

export function CamCuttingToolbar({
  disabled,
  onTwoDCut,
  onFaceOpClick,
}: CamCuttingToolbarProps) {
  return (
    <div className="flex items-center gap-1.5">
      <CamSetupButton disabled={disabled} />

      <CamToolbarDivider />

      <CamIconButton
        labelKey="cam.cutting.twoD"
        disabled={disabled}
        onClick={onTwoDCut}
      >
        <rect x="3" y="4" width="18" height="16" rx="1" />
        <path d="M3 12h18" />
      </CamIconButton>

      <CamIconButton labelKey="cam.cutting.nest" inactive>
        <rect x="3" y="3" width="8" height="8" rx="1" />
        <rect x="13" y="3" width="8" height="8" rx="1" />
        <rect x="3" y="13" width="8" height="8" rx="1" />
        <rect x="13" y="13" width="8" height="8" rx="1" />
      </CamIconButton>

      <CamIconButton labelKey="cam.cutting.leadIn" inactive>
        <path d="M18 6 6 18" />
        <circle cx="6" cy="6" r="2" />
        <circle cx="18" cy="18" r="2" />
      </CamIconButton>

      <CamToolbarDivider />

      <CamFaceOpButton disabled={disabled} onClick={onFaceOpClick} />
    </div>
  );
}
