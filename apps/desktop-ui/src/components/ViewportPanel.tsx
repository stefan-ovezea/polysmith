import type { ViewportBoxPrimitive, ViewportState } from "../types/ipc";

interface ViewportPanelProps {
  viewport: ViewportState | null;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
}

function projectPoint(x: number, y: number, z: number) {
  return {
    x: x - z * 0.55,
    y: -y - z * 0.35,
  };
}

function buildWireframe(box: ViewportBoxPrimitive) {
  const corners = {
    a: projectPoint(box.x_offset, 0, 0),
    b: projectPoint(box.x_offset + box.width, 0, 0),
    c: projectPoint(box.x_offset + box.width, box.height, 0),
    d: projectPoint(box.x_offset, box.height, 0),
    e: projectPoint(box.x_offset, 0, box.depth),
    f: projectPoint(box.x_offset + box.width, 0, box.depth),
    g: projectPoint(box.x_offset + box.width, box.height, box.depth),
    h: projectPoint(box.x_offset, box.height, box.depth),
  };

  return [
    [corners.a, corners.b, corners.c, corners.d, corners.a],
    [corners.e, corners.f, corners.g, corners.h, corners.e],
    [corners.a, corners.e],
    [corners.b, corners.f],
    [corners.c, corners.g],
    [corners.d, corners.h],
  ];
}

function toSvgPoints(points: Array<{ x: number; y: number }>) {
  return points.map((point) => `${point.x},${point.y}`).join(" ");
}

export function ViewportPanel({ viewport, onSelectPrimitive }: ViewportPanelProps) {
  if (!viewport || !viewport.has_active_document) {
    return (
      <section className="cad-panel flex h-full items-center justify-center px-6 py-6">
        <div className="text-center">
          <p className="cad-kicker">Viewport</p>
          <p className="mt-4 text-sm text-on-surface-muted">
            No active document to render.
          </p>
        </div>
      </section>
    );
  }

  if (viewport.boxes.length === 0) {
    return (
      <section className="cad-panel flex h-full items-center justify-center px-6 py-6">
        <div className="text-center">
          <p className="cad-kicker">Viewport</p>
          <p className="mt-4 text-sm text-on-surface-muted">
            No renderable primitives yet.
          </p>
        </div>
      </section>
    );
  }

  const projectedWidth = viewport.scene_width + viewport.scene_depth * 0.55;
  const projectedHeight = viewport.scene_height + viewport.scene_depth * 0.35;
  const padding = 30;
  const canvasWidth = 560;
  const canvasHeight = 320;
  const scale = Math.min(
    (canvasWidth - padding * 2) / Math.max(projectedWidth, 1),
    (canvasHeight - padding * 2) / Math.max(projectedHeight, 1),
  );

  return (
    <section className="cad-panel relative h-full overflow-hidden p-4">
      <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      <div className="relative flex items-start justify-between px-3 pb-3">
        <div>
          <p className="cad-kicker">Viewport</p>
          <h2 className="cad-title mt-2">Scene Preview</h2>
        </div>
        <div className="cad-panel-soft flex items-center gap-3 px-4 py-3">
          <div>
            <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
              Width
            </p>
            <p className="cad-metric mt-1">{viewport.scene_width.toFixed(2)} mm</p>
          </div>
          <div>
            <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
              Height
            </p>
            <p className="cad-metric mt-1">{viewport.scene_height.toFixed(2)} mm</p>
          </div>
        </div>
      </div>

      <svg
        className="relative h-[calc(100%-4rem)] w-full"
        width="100%"
        height="100%"
        viewBox={`0 0 ${canvasWidth} ${canvasHeight}`}
        style={{
          maxWidth: "100%",
        }}
      >
        <defs>
          <filter id="selectedGlow">
            <feGaussianBlur stdDeviation="4" result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>
        <g transform={`translate(${padding}, ${canvasHeight - padding}) scale(${scale})`}>
          {viewport.boxes.map((box) => {
            const segments = buildWireframe(box);
            return (
              <g key={box.primitive_id}>
                {segments.map((segment, index) => (
                  <polyline
                    key={`${box.primitive_id}-${index}`}
                    points={toSvgPoints(segment)}
                    fill="none"
                    stroke={box.is_selected ? "#00e5ff" : "#6de3ef"}
                    strokeOpacity={box.is_selected ? 1 : 0.78}
                    strokeWidth={(box.is_selected ? 2.8 : 1.35) / scale}
                    style={{ cursor: "pointer" }}
                    filter={box.is_selected ? "url(#selectedGlow)" : undefined}
                    onClick={() => {
                      void onSelectPrimitive(box.primitive_id);
                    }}
                  />
                ))}
                <text
                  x={projectPoint(box.x_offset + box.width / 2, box.height, box.depth).x}
                  y={projectPoint(box.x_offset + box.width / 2, box.height, box.depth).y - 6 / scale}
                  textAnchor="middle"
                  fontSize={12 / scale}
                  fill={box.is_selected ? "#c3f5ff" : "#9be8f2"}
                  style={{ cursor: "pointer" }}
                  onClick={() => {
                    void onSelectPrimitive(box.primitive_id);
                  }}
                >
                  {box.label}
                </text>
              </g>
            );
          })}
        </g>
      </svg>
    </section>
  );
}
