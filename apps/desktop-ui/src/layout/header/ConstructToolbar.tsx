import {
  ConstructAxisIcon,
  AnglePlaneIcon,
  ConstructPointIcon,
  HelixIcon,
  MidplaneIcon,
  OffsetPlaneIcon,
  TangentPlaneIcon,
} from "./ToolBarIcons";
import { useTranslation } from "react-i18next";

export interface ConstructToolbarProps {
  disabled: boolean;
  // True when the Offset Plane button can be clicked. Driven by the
  // parent so it knows the difference between "no document yet" and
  // "an offset-plane session is already in flight" (both disable the
  // button, but only the second case is also gated by other panels
  // being closed).
  canOffsetPlane: boolean;
  canMidplane: boolean;
  canTangentPlane: boolean;
  canAnglePlane: boolean;
  canConstructionAxis: boolean;
  canConstructionPoint: boolean;
  canHelix: boolean;
  onOffsetPlane: () => void;
  onMidplane: () => void;
  onTangentPlane: () => void;
  onAnglePlane: () => void;
  onConstructionAxis: () => void;
  onConstructionPoint: () => void;
  onHelix: () => void;
}

// See `CreateToolbar.tsx` — same icon-button base so the ribbon
// matches the Create / Sketch tabs visually.
const ICON_BUTTON_BASE = "cad-icon-button cad-icon-tool h-9 w-9 p-0";

export function ConstructToolbar({
  disabled,
  canOffsetPlane,
  canMidplane,
  canTangentPlane,
  canAnglePlane,
  canConstructionAxis,
  canConstructionPoint,
  canHelix,
  onOffsetPlane,
  onMidplane,
  onTangentPlane,
  onAnglePlane,
  onConstructionAxis,
  onConstructionPoint,
  onHelix,
}: ConstructToolbarProps) {
  const { t } = useTranslation();
  return (
    <>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.offsetPlane")}
        aria-label={t("toolbar.offsetPlane")}
        disabled={disabled || !canOffsetPlane}
        onClick={onOffsetPlane}
      >
        <OffsetPlaneIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.midplane")}
        aria-label={t("toolbar.midplane")}
        disabled={disabled || !canMidplane}
        onClick={onMidplane}
      >
        <MidplaneIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.tangentPlane")}
        aria-label={t("toolbar.tangentPlane")}
        disabled={disabled || !canTangentPlane}
        onClick={onTangentPlane}
      >
        <TangentPlaneIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.anglePlane")}
        aria-label={t("toolbar.anglePlane")}
        disabled={disabled || !canAnglePlane}
        onClick={onAnglePlane}
      >
        <AnglePlaneIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.axis")}
        aria-label={t("toolbar.axis")}
        disabled={disabled || !canConstructionAxis}
        onClick={onConstructionAxis}
      >
        <ConstructAxisIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.constructPoint")}
        aria-label={t("toolbar.constructPoint")}
        disabled={disabled || !canConstructionPoint}
        onClick={onConstructionPoint}
      >
        <ConstructPointIcon />
      </button>
      <button
        type="button"
        className={ICON_BUTTON_BASE}
        data-tooltip={t("toolbar.helix")}
        aria-label={t("toolbar.helix")}
        disabled={disabled || !canHelix}
        onClick={onHelix}
      >
        <HelixIcon />
      </button>
    </>
  );
}
