interface SketchToolPanelProps {
  activeSketchPlaneId: string;
  activeSketchTool: "select" | "line" | "rectangle" | "circle";
  selectedSketchEntityId: string | null;
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
}: SketchToolPanelProps) {
  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">Sketch</p>
      <h2 className="cad-title mt-2">{toolTitle(activeSketchTool)} Tool</h2>
      <div className="mt-4 space-y-3 text-sm text-on-surface-muted">
        <p>Plane: {activeSketchPlaneId}</p>
        <p>
          Mode:{" "}
          {activeSketchTool === "select"
            ? "Selection"
            : "Drawing"}
        </p>
        <p>
          {activeSketchTool === "line"
            ? "Click to place a line, then continue from the previous endpoint. Press Escape to stop drawing."
            : activeSketchTool === "select"
              ? "Click sketch geometry to inspect it, or pick another tool to draw."
              : "Dummy tool settings for the current sketch tool will live here."}
        </p>
        <p>
          Selection: {selectedSketchEntityId ?? "Nothing selected"}
        </p>
        <div className="rounded-2xl border border-white/10 bg-white/5 px-3 py-3">
          <p className="text-xs uppercase tracking-[0.16em] text-on-surface-dim">
            Placeholder Controls
          </p>
          <p className="mt-2">
            Construction, style, dimensions, and constraints will plug into this panel later.
          </p>
        </div>
      </div>
    </section>
  );
}
