import { useEffect, useState } from "react";
import { BoxFeatureForm } from "./BoxFeatureForm";
import { CylinderFeatureForm } from "./CylinderFeatureForm";

const workspaces = ["Create", "Modify", "Construct", "Sketch"] as const;

const modifyTools = ["Press Pull", "Fillet", "Shell", "Move"];
const constructTools = ["Offset Plane", "Midplane", "Axis", "Point"];
const sketchTools = ["Select", "Line", "Rectangle", "Circle", "Arc", "Trim"];
const syncActions = [
  { label: "Ping", key: "ping" },
  { label: "Sync Doc", key: "document" },
  { label: "Sync Session", key: "session" },
  { label: "Sync View", key: "viewport" },
] as const;

interface AppHeaderProps {
  status: string;
  disabled: boolean;
  canUndo: boolean;
  canRedo: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: "select" | "line" | "rectangle" | "circle" | null;
  selectedReferenceId: string | null;
  selectedReferenceLabel: string | null;
  sketchLineCount: number;
  sketchCircleCount: number;
  onStart: () => Promise<void>;
  onPing: () => Promise<void>;
  onCreateDocument: () => Promise<void>;
  onRefreshDocument: () => Promise<void>;
  onRefreshSession: () => Promise<void>;
  onRefreshViewport: () => Promise<void>;
  onUndo: () => Promise<void>;
  onRedo: () => Promise<void>;
  onAddBoxFeature: (width: number, height: number, depth: number) => Promise<void>;
  onAddCylinderFeature: (radius: number, height: number) => Promise<void>;
  onStartSketch: () => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onSetSketchTool: (
    tool: "select" | "line" | "rectangle" | "circle",
  ) => Promise<void>;
}

export function AppHeader({
  status,
  disabled,
  canUndo,
  canRedo,
  activeSketchPlaneId,
  activeSketchTool,
  selectedReferenceId,
  selectedReferenceLabel,
  sketchLineCount,
  sketchCircleCount,
  onStart,
  onPing,
  onCreateDocument,
  onRefreshDocument,
  onRefreshSession,
  onRefreshViewport,
  onUndo,
  onRedo,
  onAddBoxFeature,
  onAddCylinderFeature,
  onStartSketch,
  onFinishSketch,
  onSetSketchTool,
}: AppHeaderProps) {
  const [activeWorkspace, setActiveWorkspace] =
    useState<(typeof workspaces)[number]>("Create");
  const [openMenu, setOpenMenu] = useState<"box" | "cylinder" | null>(null);

  useEffect(() => {
    setOpenMenu(null);
  }, [activeWorkspace]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      setActiveWorkspace("Sketch");
    }
  }, [activeSketchPlaneId]);

  return (
    <header className="cad-ribbon relative z-20">
      <div className="flex items-center justify-between gap-5 px-5 py-2">
        <div className="flex items-center gap-8">
          <div>
            <p className="font-display text-[1.35rem] font-bold uppercase tracking-[0.08em] text-primary-glow">
              PolySmith
            </p>
          </div>
          <nav className="flex items-center gap-1 rounded-full p-0.5 cad-subtle-block">
            {workspaces.map((workspace) => (
              <button
                key={workspace}
                className={
                  activeWorkspace === workspace
                    ? "cad-ribbon-tab cad-ribbon-tab-active"
                    : "cad-ribbon-tab"
                }
                onClick={() => {
                  setActiveWorkspace(workspace);
                }}
              >
                {workspace}
              </button>
            ))}
          </nav>
        </div>

        <div className="flex items-center gap-2">
          {status !== "connected" ? (
            <button className="cad-ribbon-action cad-ribbon-action-primary" onClick={() => void onStart()}>
              Start Core
            </button>
          ) : null}
          <button className="cad-ribbon-action" onClick={() => void onCreateDocument()} disabled={disabled}>
            New
          </button>
          <button className="cad-ribbon-action" onClick={() => void onUndo()} disabled={disabled || !canUndo}>
            Undo
          </button>
          <button className="cad-ribbon-action" onClick={() => void onRedo()} disabled={disabled || !canRedo}>
            Redo
          </button>
          <div className="cad-status-pill">
            <span
              className={`h-2.5 w-2.5 rounded-full ${
                status === "connected"
                  ? "bg-success cad-status-dot-online"
                  : "bg-danger cad-status-dot-offline"
              }`}
            />
            <span>{status === "connected" ? "Local Session" : "Core Offline"}</span>
          </div>
        </div>
      </div>

      <div className="flex items-center justify-between gap-5 px-5 py-2" style={{ borderTop: "1px solid var(--cad-panel-soft-border)" }}>
        <div className="flex min-w-0 items-center gap-3">
          {activeWorkspace === "Create" ? (
            <>
              <div className="relative flex items-center gap-1.5">
                <button
                  className={openMenu === "box" ? "cad-tool-button cad-tool-button-active" : "cad-tool-button"}
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
                    setOpenMenu((current) => (current === "cylinder" ? null : "cylinder"));
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
          ) : null}

          {activeWorkspace === "Modify"
            ? modifyTools.map((tool) => (
                <button key={tool} className="cad-tool-button" disabled>
                  {tool}
                </button>
              ))
            : null}

          {activeWorkspace === "Construct"
            ? constructTools.map((tool) => (
                <button key={tool} className="cad-tool-button" disabled>
                  {tool}
                </button>
              ))
            : null}

          {activeWorkspace === "Sketch" ? (
            <>
              <button
                className={activeSketchPlaneId ? "cad-tool-button cad-tool-button-active" : "cad-tool-button"}
                onClick={() => {
                  void (activeSketchPlaneId ? onFinishSketch() : onStartSketch());
                }}
                disabled={disabled || (!activeSketchPlaneId && !selectedReferenceId)}
              >
                {activeSketchPlaneId ? "Finish Sketch" : "Start Sketch"}
              </button>
              <div className="cad-tool-group-label">
                {activeSketchPlaneId
                  ? `${activeSketchPlaneId} · ${sketchLineCount} line${sketchLineCount === 1 ? "" : "s"} · ${sketchCircleCount} circle${sketchCircleCount === 1 ? "" : "s"}`
                  : selectedReferenceLabel ?? "Select a plane to sketch"}
              </div>
              {sketchTools.map((tool) => (
                <button
                  key={tool}
                  className={
                    activeSketchPlaneId &&
                    activeSketchTool === tool.toLowerCase()
                      ? "cad-tool-button cad-tool-button-active"
                      : "cad-tool-button"
                  }
                  disabled={
                    !activeSketchPlaneId ||
                    (tool !== "Line" && tool !== "Rectangle" && tool !== "Circle")
                  }
                  onClick={() => {
                    if (!activeSketchPlaneId) {
                      return;
                    }

                    if (
                      tool === "Select" ||
                      tool === "Line" ||
                      tool === "Rectangle" ||
                      tool === "Circle"
                    ) {
                      void onSetSketchTool(
                        tool.toLowerCase() as
                          | "select"
                          | "line"
                          | "rectangle"
                          | "circle",
                      );
                    }
                  }}
                >
                  {tool}
                </button>
              ))}
            </>
          ) : null}
        </div>

        <div className="flex items-center gap-1.5">
          {syncActions.map((action) => (
            <button
              key={action.key}
              className="cad-ribbon-action"
              onClick={() => {
                if (action.key === "ping") {
                  void onPing();
                  return;
                }

                if (action.key === "document") {
                  void onRefreshDocument();
                  return;
                }

                if (action.key === "session") {
                  void onRefreshSession();
                  return;
                }

                void onRefreshViewport();
              }}
              disabled={disabled}
            >
              {action.label}
            </button>
          ))}
        </div>
      </div>
    </header>
  );
}
