import { SketchTool } from "@/types";
import { useState } from "react";

interface SketchToolPanelProps {
  activeSketchPlaneId: string;
  activeSketchTool: SketchTool | null;
  selectedSketchEntityId: string | null;
  selectedSketchProfileId: string | null;
  onExtrudeProfile: (depth: number) => Promise<void>;
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
  selectedSketchEntityId,
  selectedSketchProfileId,
  onExtrudeProfile,
}: SketchToolPanelProps) {
  const [depth, setDepth] = useState("20");

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">Sketch</p>
      <h2 className="cad-title mt-2">{toolTitle(activeSketchTool)} Tool</h2>
      <div className="mt-4 space-y-3 text-sm text-on-surface-muted">
        <p>Plane: {activeSketchPlaneId}</p>
        <p>Selection: {selectedSketchEntityId ?? "Nothing selected"}</p>
        <p>Profile: {selectedSketchProfileId ?? "No profile selected"}</p>
        {selectedSketchProfileId ? (
          <form
            className="space-y-3 rounded-2xl border border-white/10 bg-white/5 px-3 py-3"
            onSubmit={(event) => {
              event.preventDefault();
              void onExtrudeProfile(Number(depth));
            }}
          >
            <p className="text-xs uppercase tracking-[0.16em] text-on-surface-dim">
              Extrude
            </p>
            <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              Depth
              <input
                className="cad-input mt-2"
                type="number"
                min="0.01"
                step="0.01"
                value={depth}
                onChange={(event) => {
                  setDepth(event.target.value);
                }}
              />
            </label>
            <button className="cad-action-primary min-w-[140px]" type="submit">
              Extrude Profile
            </button>
          </form>
        ) : null}
        <div className="rounded-2xl border border-white/10 bg-white/5 px-3 py-3">
          <p className="text-xs uppercase tracking-[0.16em] text-on-surface-dim">
            Placeholder Controls
          </p>
        </div>
      </div>
    </section>
  );
}
