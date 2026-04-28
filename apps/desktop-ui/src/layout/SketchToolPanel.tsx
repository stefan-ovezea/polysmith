import { SketchTool } from "@/types";

interface SketchToolPanelProps {
  activeSketchPlaneId: string;
  activeSketchTool: SketchTool | null;
  selectedSketchPointId: string | null;
  selectedSketchEntityId: string | null;
  selectedSketchProfileId: string | null;
}

function toolTitle(tool: SketchToolPanelProps["activeSketchTool"]) {
  if (tool === "select") {
    return "Select";
  }

  if (tool === "line") {
    return "Line";
  }

  if (tool === "rectangle") {
    return "Rectangle";
  }

  return "Circle";
}

export function SketchToolPanel({
  activeSketchPlaneId,
  activeSketchTool,
  selectedSketchPointId,
  selectedSketchEntityId,
  selectedSketchProfileId,
}: SketchToolPanelProps) {
  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">Sketch</p>
      <h2 className="cad-title mt-2">{toolTitle(activeSketchTool)} Tool</h2>
      <div className="mt-4 space-y-3 text-sm text-on-surface-muted">
        <p>Plane: {activeSketchPlaneId}</p>
        <p>
          Selection:{" "}
          {selectedSketchPointId
            ? `Point ${selectedSketchPointId}`
            : (selectedSketchEntityId ?? "Nothing selected")}
        </p>
        <p>Profile: {selectedSketchProfileId ?? "No profile selected"}</p>
        {selectedSketchProfileId ? (
          <p className="text-xs uppercase tracking-[0.16em] text-on-surface-dim">
            Press E to extrude this profile
          </p>
        ) : null}
      </div>
    </section>
  );
}
