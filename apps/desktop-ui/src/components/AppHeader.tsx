import { useEffect, useState } from "react";
import { BoxFeatureForm } from "./BoxFeatureForm";
import { CylinderFeatureForm } from "./CylinderFeatureForm";

const workspaces = ["Create", "Modify", "Construct", "Sketch"] as const;

const modifyTools = ["Press Pull", "Fillet", "Shell", "Move"];
const constructTools = ["Offset Plane", "Midplane", "Axis", "Point"];
type SketchToolbarTool =
  | "select"
  | "line"
  | "rectangle"
  | "circle"
  | "arc"
  | "trim";

const sketchTools: Array<{
  id: SketchToolbarTool;
  label: string;
  shortcut?: string;
  enabled: boolean;
}> = [
  { id: "select", label: "Select", enabled: true },
  { id: "line", label: "Line", shortcut: "L", enabled: true },
  { id: "rectangle", label: "Rectangle", shortcut: "R", enabled: true },
  { id: "circle", label: "Circle", shortcut: "C", enabled: true },
  { id: "arc", label: "Arc", enabled: false },
  { id: "trim", label: "Trim", enabled: false },
];

function SketchToolIcon({ tool }: { tool: SketchToolbarTool }) {
  if (tool === "select") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M6 4.5v14l4.2-3 2.2 4.2 2.1-1.1-2.2-4.2 4.7-.7L6 4.5Z" />
      </svg>
    );
  }

  if (tool === "line") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <circle cx="6" cy="18" r="1.6" />
        <circle cx="18" cy="6" r="1.6" />
        <path d="M7.4 16.6 16.6 7.4" />
      </svg>
    );
  }

  if (tool === "rectangle") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <rect x="5" y="7" width="14" height="10" rx="1.5" />
      </svg>
    );
  }

  if (tool === "circle") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <circle cx="12" cy="12" r="6.5" />
      </svg>
    );
  }

  if (tool === "arc") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M6 15a6 6 0 1 1 12 0" />
        <circle cx="6" cy="15" r="1.4" />
        <circle cx="18" cy="15" r="1.4" />
      </svg>
    );
  }

  return (
    <svg
      viewBox="0 0 24 24"
      className="h-4 w-4"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="m7 17 10-10" />
      <path d="m9 7 8 8" />
    </svg>
  );
}

function ConstraintIcon({
  kind,
}: {
  kind:
    | "horizontal"
    | "vertical"
    | "clear"
    | "coincident"
    | "equal_length"
    | "perpendicular"
    | "parallel";
}) {
  if (kind === "horizontal") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M5 12h14" />
        <path d="m8 9-3 3 3 3" />
        <path d="m16 9 3 3-3 3" />
      </svg>
    );
  }

  if (kind === "vertical") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M12 5v14" />
        <path d="m9 8 3-3 3 3" />
        <path d="m9 16 3 3 3-3" />
      </svg>
    );
  }

  if (kind === "perpendicular") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M7 6v12" />
        <path d="M7 12h10" />
      </svg>
    );
  }

  if (kind === "coincident") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <circle cx="8.5" cy="12" r="3.2" />
        <circle cx="15.5" cy="12" r="3.2" />
      </svg>
    );
  }

  if (kind === "parallel") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M7 8h10" />
        <path d="M7 16h10" />
        <path d="m9 6-2 2 2 2" />
        <path d="m15 14 2 2-2 2" />
      </svg>
    );
  }

  if (kind === "equal_length") {
    return (
      <svg
        viewBox="0 0 24 24"
        className="h-4 w-4"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
        aria-hidden="true"
      >
        <path d="M6 9h12" />
        <path d="M6 15h12" />
      </svg>
    );
  }

  return (
    <svg
      viewBox="0 0 24 24"
      className="h-4 w-4"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="m7 7 10 10" />
      <path d="m17 7-10 10" />
    </svg>
  );
}

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
  armedSketchConstraint:
    | null
    | { kind: "horizontal" | "vertical" | "clear" }
    | {
        kind: "equal_length" | "perpendicular" | "parallel";
        firstLineId: string | null;
      }
    | { kind: "coincident"; firstPointId: string | null };
  onStart: () => Promise<void>;
  onCreateDocument: () => Promise<void>;
  onExportDocument: () => Promise<void>;
  onUndo: () => Promise<void>;
  onRedo: () => Promise<void>;
  onAddBoxFeature: (
    width: number,
    height: number,
    depth: number,
  ) => Promise<void>;
  onAddCylinderFeature: (radius: number, height: number) => Promise<void>;
  onStartSketch: () => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onSetSketchTool: (
    tool: "select" | "line" | "rectangle" | "circle",
  ) => Promise<void>;
  onArmSketchConstraint: (
    constraint:
      | "horizontal"
      | "vertical"
      | "clear"
      | "equal_length"
      | "coincident"
      | "perpendicular"
      | "parallel",
  ) => Promise<void>;
  onCancelSketchConstraint: () => void;
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
  armedSketchConstraint,
  onStart,
  onCreateDocument,
  onExportDocument,
  onUndo,
  onRedo,
  onAddBoxFeature,
  onAddCylinderFeature,
  onStartSketch,
  onFinishSketch,
  onSetSketchTool,
  onArmSketchConstraint,
  onCancelSketchConstraint,
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
            <button
              className="cad-ribbon-action cad-ribbon-action-primary"
              onClick={() => void onStart()}
            >
              Start Core
            </button>
          ) : null}
          <button
            className="cad-ribbon-action"
            onClick={() => void onCreateDocument()}
            disabled={disabled}
          >
            New
          </button>
          <button
            className="cad-ribbon-action"
            onClick={() => void onExportDocument()}
            disabled={disabled}
          >
            Export STEP
          </button>
          <button
            className="cad-ribbon-action"
            onClick={() => void onUndo()}
            disabled={disabled || !canUndo}
          >
            Undo
          </button>
          <button
            className="cad-ribbon-action"
            onClick={() => void onRedo()}
            disabled={disabled || !canRedo}
          >
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
            <span>
              {status === "connected" ? "Local Session" : "Core Offline"}
            </span>
          </div>
        </div>
      </div>

      <div
        className="flex items-center justify-between gap-5 px-5 py-2"
        style={{ borderTop: "1px solid var(--cad-panel-soft-border)" }}
      >
        <div className="flex min-w-0 items-center gap-3">
          {activeWorkspace === "Create" ? (
            <>
              <div className="relative flex items-center gap-1.5">
                <button
                  className={
                    openMenu === "box"
                      ? "cad-tool-button cad-tool-button-active"
                      : "cad-tool-button"
                  }
                  onClick={() => {
                    setOpenMenu((current) =>
                      current === "box" ? null : "box",
                    );
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
                className={
                  activeSketchPlaneId
                    ? "cad-tool-button cad-tool-button-active"
                    : "cad-tool-button"
                }
                onClick={() => {
                  void (activeSketchPlaneId
                    ? onFinishSketch()
                    : onStartSketch());
                }}
                disabled={
                  disabled || (!activeSketchPlaneId && !selectedReferenceId)
                }
              >
                {activeSketchPlaneId ? "Finish Sketch" : "Start Sketch"}
              </button>
              <div className="cad-tool-group-label">
                {activeSketchPlaneId
                  ? `${activeSketchPlaneId} · ${sketchLineCount} line${sketchLineCount === 1 ? "" : "s"} · ${sketchCircleCount} circle${sketchCircleCount === 1 ? "" : "s"}`
                  : (selectedReferenceLabel ?? "Select a plane to sketch")}
              </div>
              {sketchTools.map((tool) => (
                <button
                  key={tool.id}
                  className={
                    activeSketchPlaneId && activeSketchTool === tool.id
                      ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                      : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                  }
                  data-tooltip={
                    tool.shortcut
                      ? `${tool.label} (${tool.shortcut})`
                      : tool.label
                  }
                  aria-label={
                    tool.shortcut
                      ? `${tool.label} (${tool.shortcut})`
                      : tool.label
                  }
                  disabled={!activeSketchPlaneId || !tool.enabled}
                  onClick={() => {
                    if (
                      !activeSketchPlaneId ||
                      !tool.enabled ||
                      (tool.id !== "select" &&
                        tool.id !== "line" &&
                        tool.id !== "rectangle" &&
                        tool.id !== "circle")
                    ) {
                      return;
                    }

                    onCancelSketchConstraint();
                    void onSetSketchTool(tool.id);
                  }}
                >
                  <SketchToolIcon tool={tool.id} />
                </button>
              ))}
              <div className="h-8 w-px bg-white/10" />
              <div className="cad-tool-group-label">Constraints</div>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "horizontal"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Horizontal"
                aria-label="Horizontal"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("horizontal");
                }}
              >
                <ConstraintIcon kind="horizontal" />
              </button>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "vertical"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Vertical"
                aria-label="Vertical"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("vertical");
                }}
              >
                <ConstraintIcon kind="vertical" />
              </button>
              <button
                className={
                  activeSketchPlaneId && armedSketchConstraint?.kind === "clear"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Clear Constraint"
                aria-label="Clear Constraint"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("clear");
                }}
              >
                <ConstraintIcon kind="clear" />
              </button>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "coincident"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Coincident"
                aria-label="Coincident"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("coincident");
                }}
              >
                <ConstraintIcon kind="coincident" />
              </button>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "equal_length"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Equal Length"
                aria-label="Equal Length"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("equal_length");
                }}
              >
                <ConstraintIcon kind="equal_length" />
              </button>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "perpendicular"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Perpendicular"
                aria-label="Perpendicular"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("perpendicular");
                }}
              >
                <ConstraintIcon kind="perpendicular" />
              </button>
              <button
                className={
                  activeSketchPlaneId &&
                  armedSketchConstraint?.kind === "parallel"
                    ? "cad-icon-button cad-tool-button cad-tool-button-active h-9 w-9 px-0"
                    : "cad-icon-button cad-tool-button h-9 w-9 px-0"
                }
                data-tooltip="Parallel"
                aria-label="Parallel"
                disabled={!activeSketchPlaneId}
                onClick={() => {
                  void onArmSketchConstraint("parallel");
                }}
              >
                <ConstraintIcon kind="parallel" />
              </button>
              {armedSketchConstraint ? (
                <p className="text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                  {armedSketchConstraint.kind === "coincident"
                    ? armedSketchConstraint.firstPointId
                      ? "Coincident: click second point"
                      : "Coincident: click first point"
                    : armedSketchConstraint.kind === "equal_length" ||
                        armedSketchConstraint.kind === "perpendicular" ||
                        armedSketchConstraint.kind === "parallel"
                      ? armedSketchConstraint.firstLineId
                        ? `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"}: click second line`
                        : `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"}: click first line`
                      : `${armedSketchConstraint.kind}: click line`}
                </p>
              ) : null}
            </>
          ) : null}
        </div>

        <div />
      </div>
    </header>
  );
}
