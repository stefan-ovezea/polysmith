import { useEffect, useState } from "react";
import { ConstraintType, SketchTool, ArmedSketchConstraint } from "@/types";
import { SketchToolbar } from "./SketchToolbar";
import { CreateToolbar } from "./CreateToolbar";
import { ModifyToolbar } from "./ModifyToolbar";
import { ConstructToolbar } from "./ConstructToolbar";

const workspaces = ["Create", "Modify", "Construct", "Sketch"] as const;

interface AppHeaderProps {
  status: string;
  disabled: boolean;
  canUndo: boolean;
  canRedo: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  selectedReferenceId: string | null;
  selectedReferenceLabel: string | null;
  sketchLineCount: number;
  sketchCircleCount: number;
  armedSketchConstraint: ArmedSketchConstraint;
  onStart: () => Promise<void>;
  onCreateDocument: () => Promise<void>;
  onExportDocument: () => Promise<void>;
  onExportDocumentStl: () => Promise<void>;
  onSaveDocument: () => Promise<void>;
  onLoadDocument: () => Promise<void>;
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
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
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
  onExportDocumentStl,
  onSaveDocument,
  onLoadDocument,
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
            onClick={() => void onLoadDocument()}
            disabled={disabled}
          >
            Open
          </button>
          <button
            className="cad-ribbon-action"
            onClick={() => void onSaveDocument()}
            disabled={disabled}
          >
            Save
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
            onClick={() => void onExportDocumentStl()}
            disabled={disabled}
          >
            Export STL
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
            <CreateToolbar
              openMenu={openMenu}
              disabled={disabled}
              setOpenMenu={setOpenMenu}
              onAddBoxFeature={onAddBoxFeature}
              onAddCylinderFeature={onAddCylinderFeature}
            />
          ) : null}

          {activeWorkspace === "Modify" ? <ModifyToolbar /> : null}

          {activeWorkspace === "Construct" ? <ConstructToolbar /> : null}

          {activeWorkspace === "Sketch" ? (
            <SketchToolbar
              activeSketchPlaneId={activeSketchPlaneId}
              activeSketchTool={activeSketchTool}
              selectedReferenceId={selectedReferenceId}
              selectedReferenceLabel={selectedReferenceLabel}
              sketchLineCount={sketchLineCount}
              sketchCircleCount={sketchCircleCount}
              armedSketchConstraint={armedSketchConstraint}
              onStartSketch={onStartSketch}
              onFinishSketch={onFinishSketch}
              onCancelSketchConstraint={onCancelSketchConstraint}
              onSetSketchTool={onSetSketchTool}
              onArmSketchConstraint={onArmSketchConstraint}
            />
          ) : null}
        </div>

        <div />
      </div>
    </header>
  );
}
