import { SketchTool } from "@/types";

interface SketchToolPanelProps {
  activeSketchPlaneId: string;
  activeSketchTool: SketchTool | null;
  selectedSketchPointId: string | null;
  selectedSketchEntityId: string | null;
  selectedSketchProfileId: string | null;
  selectedFaceId: string | null;
  onProjectFace: () => Promise<void>;
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
  selectedFaceId,
  onProjectFace,
}: SketchToolPanelProps) {
  const canProject = selectedFaceId !== null;
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
        <div className="pt-2">
          <button
            type="button"
            className="cad-action-primary w-full disabled:opacity-50"
            onClick={() => void onProjectFace()}
            disabled={!canProject}
            title={
              canProject
                ? "Project the selected face onto the active sketch"
                : "Select a face on an existing extrude to project it"
            }
          >
            Project selected face
          </button>
          {!canProject ? (
            <p className="mt-2 text-xs uppercase tracking-[0.16em] text-on-surface-dim">
              Select an extrude face first
            </p>
          ) : null}
        </div>
      </div>
    </section>
  );
}
