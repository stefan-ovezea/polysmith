import { BoxFeatureForm } from "../BoxFeatureForm";
import { CylinderFeatureForm } from "../CylinderFeatureForm";

export interface CreateToolbarProps {
  openMenu: "box" | "cylinder" | null;
  disabled: boolean;
  setOpenMenu: React.Dispatch<React.SetStateAction<"box" | "cylinder" | null>>;
  onAddBoxFeature: (
    width: number,
    height: number,
    depth: number,
  ) => Promise<void>;
  onAddCylinderFeature: (radius: number, height: number) => Promise<void>;
}

export function CreateToolbar({
  openMenu,
  disabled,
  setOpenMenu,
  onAddBoxFeature,
  onAddCylinderFeature,
}: CreateToolbarProps) {
  return (
    <>
      <div className="relative flex items-center gap-1.5">
        <button
          className={
            openMenu === "box"
              ? "cad-tool-button cad-tool-button-active"
              : "cad-tool-button"
          }
          onClick={() => {
            setOpenMenu((current) => (current === "box" ? null : "box"));
          }}
          disabled={disabled}
        >
          Box
        </button>
        <button
          className={
            openMenu === "cylinder"
              ? "cad-tool-button cad-tool-button-active"
              : "cad-tool-button"
          }
          onClick={() => {
            setOpenMenu((current) =>
              current === "cylinder" ? null : "cylinder",
            );
          }}
          disabled={disabled}
        >
          Cylinder
        </button>
        {openMenu === "box" ? (
          <div className="cad-toolbar-popover absolute left-0 top-[calc(100%+0.75rem)] w-[360px]">
            <BoxFeatureForm
              disabled={disabled}
              onSubmit={async (width, height, depth) => {
                await onAddBoxFeature(width, height, depth);
                setOpenMenu(null);
              }}
              variant="toolbar"
            />
          </div>
        ) : null}
        {openMenu === "cylinder" ? (
          <div className="cad-toolbar-popover absolute left-[8.5rem] top-[calc(100%+0.75rem)] w-[320px]">
            <CylinderFeatureForm
              disabled={disabled}
              onSubmit={async (radius, height) => {
                await onAddCylinderFeature(radius, height);
                setOpenMenu(null);
              }}
              variant="toolbar"
            />
          </div>
        ) : null}
      </div>
      <div className="cad-tool-group-label">Primitives</div>
      <button className="cad-tool-button" disabled>
        Sphere
      </button>
      <button className="cad-tool-button" disabled>
        Loft
      </button>
      <button className="cad-tool-button" disabled>
        Pattern
      </button>
    </>
  );
}
