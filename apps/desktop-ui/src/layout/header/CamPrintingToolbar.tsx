import {
  CamFaceOpButton,
  CamIconButton,
  CamSetupButton,
  CamToolbarDivider,
} from "./CamToolbarControls";

export interface CamPrintingToolbarProps {
  disabled: boolean;
}

export function CamPrintingToolbar({ disabled }: CamPrintingToolbarProps) {
  return (
    <div className="flex items-center gap-1.5">
      <CamSetupButton disabled={disabled} />

      <CamToolbarDivider />

      <CamIconButton labelKey="cam.printing.slice" inactive>
        <rect x="3" y="3" width="18" height="18" rx="2" />
        <path d="M3 9h18M3 15h18" />
      </CamIconButton>

      <CamIconButton labelKey="cam.printing.support" inactive>
        <path d="M12 20V4M8 20l4-4 4 4" />
        <path d="M6 4h12" />
      </CamIconButton>

      <CamIconButton labelKey="cam.printing.infill" inactive>
        <rect x="3" y="3" width="18" height="18" rx="2" />
        <path d="M3 3l18 18M21 3L3 21" />
      </CamIconButton>

      <CamToolbarDivider />

      <CamFaceOpButton />
    </div>
  );
}
