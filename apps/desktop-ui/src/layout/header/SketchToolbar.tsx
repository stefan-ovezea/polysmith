import { ConstraintType, SketchTool, ArmedSketchConstraint } from "@/types";
import { ConstraintIcon, SketchToolIcon } from "./ToolBarIcons";

interface SketchToolbarProps {
  disabled?: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  selectedReferenceId: string | null;
  selectedReferenceLabel: string | null;
  sketchLineCount: number;
  sketchCircleCount: number;
  armedSketchConstraint: ArmedSketchConstraint;

  onStartSketch: () => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onCancelSketchConstraint: () => void;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
}

const sketchTools: Array<{
  id: SketchTool;
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

export function SketchToolbar({
  disabled = false,
  activeSketchPlaneId,
  activeSketchTool,
  selectedReferenceId,
  selectedReferenceLabel,
  sketchLineCount,
  sketchCircleCount,
  armedSketchConstraint,
  onStartSketch,
  onFinishSketch,
  onCancelSketchConstraint,
  onSetSketchTool,
  onArmSketchConstraint,
}: SketchToolbarProps) {
  return (
    <>
      <button
        className={
          activeSketchPlaneId
            ? "cad-tool-button cad-tool-button-active"
            : "cad-tool-button"
        }
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
            tool.shortcut ? `${tool.label} (${tool.shortcut})` : tool.label
          }
          aria-label={
            tool.shortcut ? `${tool.label} (${tool.shortcut})` : tool.label
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "horizontal"
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "vertical"
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "coincident"
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "equal_length"
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "perpendicular"
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
          activeSketchPlaneId && armedSketchConstraint?.kind === "parallel"
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
  );
}
