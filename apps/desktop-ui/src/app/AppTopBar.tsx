import type { Dispatch, SetStateAction } from "react";

import { AppHeader } from "../layout";
import type {
  ArmedSketchConstraint,
  DocumentState,
  SelectionFilterUpdate,
  SketchTool,
  ViewportState,
} from "../types";
import type { CamOperationType } from "../layout/header/CamToolbar";
import type { PendingUnsavedAction, WorkspaceView } from "./appState";
import {
  pickExportPath,
  pickLoadDocumentPath,
  type DialogTranslate,
} from "./documentDialogs";
import { toggleArmedSketchConstraint } from "./sketchConstraintArm";

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;
type EdgeOperationKind = "fillet" | "chamfer";

interface AppTopBarProps {
  workspaceView: WorkspaceView;
  canOpenSlicerView: boolean;
  canExportToSlicer: boolean;
  showCadView: AsyncVoid;
  showCamView: AsyncVoid;
  showSlicerView: AsyncVoid;
  exportToSlicer: () => Promise<unknown>;
  status: string;
  canUndo: boolean;
  canRedo: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  selectedReferenceId: string | null;
  selectedFaceId: string | null;
  armedSketchConstraint: ArmedSketchConstraint;
  isMirrorToolOpen: boolean;
  arcToolMode: "three_point" | "center_start_end";
  setArcToolMode: (mode: "three_point" | "center_start_end") => void;
  rectangleToolMode: "corner_corner" | "center_point" | "three_point";
  setRectangleToolMode: (
    mode: "corner_corner" | "center_point" | "three_point",
  ) => void;
  circleToolMode:
    | "center_radius"
    | "two_point"
    | "three_point"
    | "tangent_two_lines"
    | "tangent_three_lines";
  setCircleToolMode: (
    mode:
      | "center_radius"
      | "two_point"
      | "three_point"
      | "tangent_two_lines"
      | "tangent_three_lines",
  ) => void;
  polygonToolMode: "circumscribed" | "inscribed" | "edge";
  setPolygonToolMode: (
    mode: "circumscribed" | "inscribed" | "edge",
  ) => void;
  runAction: RunAction;
  start: AsyncVoid;
  startMirrorPreview: AsyncVoid;
  setMirrorFocusedSlot: Dispatch<
    SetStateAction<"objects" | "axis" | null>
  >;
  clearArmedSketchConstraint: () => void;
  requestUnsavedGate: (action: PendingUnsavedAction) => void;
  translate: DialogTranslate;
  document: DocumentState | null;
  viewport: ViewportState | null;
  addMessage: (message: string) => void;
  exportDocument: (filePath: string) => Promise<void>;
  saveCurrentDocument: () => Promise<unknown>;
  undo: AsyncVoid;
  redo: AsyncVoid;
  logCount: number;
  errorLogCount: number;
  setIsLogsOpen: Dispatch<SetStateAction<boolean>>;
  setIsSettingsOpen: Dispatch<SetStateAction<boolean>>;
  showAiAssistant: boolean;
  isAiPanelOpen: boolean;
  setIsAiPanelOpen: Dispatch<SetStateAction<boolean>>;
  addBoxFeature: (
    width: number,
    height: number,
    depth: number,
  ) => Promise<void>;
  addCylinderFeature: (radius: number, height: number) => Promise<void>;
  canExtrudeFromSelection: boolean;
  triggerExtrudeAction: AsyncVoid;
  canStartSolidFeatureAction: boolean;
  triggerLoftAction: AsyncVoid;
  triggerRevolveAction: AsyncVoid;
  triggerSweepAction: AsyncVoid;
  triggerHoleAction: AsyncVoid;
  triggerThreadAction: AsyncVoid;
  triggerFastenerAction: AsyncVoid;
  triggerEdgeOpAction: (kind: EdgeOperationKind) => Promise<void>;
  triggerMoveAction: AsyncVoid;
  triggerShellAction: AsyncVoid;
  canStartReferencePlaneAction: boolean;
  triggerOffsetPlaneAction: AsyncVoid;
  triggerMidplaneAction: AsyncVoid;
  triggerTangentPlaneAction: AsyncVoid;
  triggerAnglePlaneAction: AsyncVoid;
  canStartConstructionReferenceAction: boolean;
  triggerConstructionAxisAction: AsyncVoid;
  triggerConstructionPointAction: AsyncVoid;
  canStartHelixRibbonAction: boolean;
  triggerHelixAction: AsyncVoid;
  triggerCreateSketchAction: AsyncVoid;
  finishActiveSketch: AsyncVoid;
  setActiveSketchTool: (tool: SketchTool) => Promise<void>;
  setArmedSketchConstraint: Dispatch<SetStateAction<ArmedSketchConstraint>>;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  handleWorkspaceDropdownOpenChange: (isOpen: boolean) => void;
  parametersPanelOpen: boolean;
  setParametersPanelOpen: Dispatch<SetStateAction<boolean>>;
  filterPanelOpen: boolean;
  setFilterPanelOpen: Dispatch<SetStateAction<boolean>>;
  materialsPanelOpen: boolean;
  setMaterialsPanelOpen: Dispatch<SetStateAction<boolean>>;
  updateSelectionFilter: (filter: SelectionFilterUpdate) => Promise<void>;
  activeCamOperation: CamOperationType | null;
  setActiveCamOperation: Dispatch<SetStateAction<CamOperationType | null>>;
  setIsCamSetupPanelOpen: Dispatch<SetStateAction<boolean>>;
  camFaceMillingCreate: (
    bodyId: string,
    faceIndex: number,
  ) => Promise<void>;
}

export function AppTopBar({
  workspaceView,
  canOpenSlicerView,
  canExportToSlicer,
  showCadView,
  showCamView,
  showSlicerView,
  exportToSlicer,
  status,
  canUndo,
  canRedo,
  activeSketchPlaneId,
  activeSketchTool,
  selectedReferenceId,
  selectedFaceId,
  armedSketchConstraint,
  isMirrorToolOpen,
  arcToolMode,
  setArcToolMode,
  rectangleToolMode,
  setRectangleToolMode,
  circleToolMode,
  setCircleToolMode,
  polygonToolMode,
  setPolygonToolMode,
  runAction,
  start,
  startMirrorPreview,
  setMirrorFocusedSlot,
  clearArmedSketchConstraint,
  requestUnsavedGate,
  translate,
  document,
  viewport,
  addMessage,
  exportDocument,
  saveCurrentDocument,
  undo,
  redo,
  logCount,
  errorLogCount,
  setIsLogsOpen,
  setIsSettingsOpen,
  showAiAssistant,
  isAiPanelOpen,
  setIsAiPanelOpen,
  addBoxFeature,
  addCylinderFeature,
  canExtrudeFromSelection,
  triggerExtrudeAction,
  canStartSolidFeatureAction,
  triggerLoftAction,
  triggerRevolveAction,
  triggerSweepAction,
  triggerHoleAction,
  triggerThreadAction,
  triggerFastenerAction,
  triggerEdgeOpAction,
  triggerMoveAction,
  triggerShellAction,
  canStartReferencePlaneAction,
  triggerOffsetPlaneAction,
  triggerMidplaneAction,
  triggerTangentPlaneAction,
  triggerAnglePlaneAction,
  canStartConstructionReferenceAction,
  triggerConstructionAxisAction,
  triggerConstructionPointAction,
  canStartHelixRibbonAction,
  triggerHelixAction,
  triggerCreateSketchAction,
  finishActiveSketch,
  setActiveSketchTool,
  setArmedSketchConstraint,
  setSketchTool,
  handleWorkspaceDropdownOpenChange,
  parametersPanelOpen,
  setParametersPanelOpen,
  filterPanelOpen,
  setFilterPanelOpen,
  materialsPanelOpen,
  setMaterialsPanelOpen,
  updateSelectionFilter,
  activeCamOperation,
  setActiveCamOperation,
  setIsCamSetupPanelOpen,
  camFaceMillingCreate,
}: AppTopBarProps) {
  return (
    <AppHeader
      workspaceView={workspaceView}
      canOpenSlicerView={canOpenSlicerView}
      canExportToSlicer={canExportToSlicer}
      onSetWorkspaceView={(view) => {
        if (view === "cad") {
          void showCadView();
          return;
        }
        if (view === "cam") {
          void showCamView();
          return;
        }
        void showSlicerView();
      }}
      onExportToSlicer={() => void exportToSlicer()}
      status={status}
      disabled={status !== "connected"}
      canUndo={canUndo}
      canRedo={canRedo}
      activeSketchPlaneId={activeSketchPlaneId}
      activeSketchTool={activeSketchTool}
      selectedReferenceId={selectedReferenceId}
      selectedFaceId={selectedFaceId}
      armedSketchConstraint={armedSketchConstraint}
      isMirrorToolOpen={isMirrorToolOpen}
      arcToolMode={arcToolMode}
      onSetArcToolMode={setArcToolMode}
      rectangleToolMode={rectangleToolMode}
      onSetRectangleToolMode={setRectangleToolMode}
      circleToolMode={circleToolMode}
      onSetCircleToolMode={setCircleToolMode}
      polygonToolMode={polygonToolMode}
      onSetPolygonToolMode={setPolygonToolMode}
      onStart={async () => {
        await runAction(start);
      }}
      onStartMirrorTool={async () => {
        await runAction(async () => {
          await startMirrorPreview();
          setMirrorFocusedSlot("objects");
          clearArmedSketchConstraint();
        });
      }}
      onCreateDocument={async () => {
        requestUnsavedGate({ kind: "new" });
      }}
      onExportDocument={async () => {
        const filePath = await pickExportPath({
          translate,
          documentName: document?.name,
          addMessage,
        });
        if (!filePath) {
          return;
        }

        await runAction(async () => {
          await exportDocument(filePath);
          addMessage(`export requested: ${filePath}`);
        });
      }}
      onSaveDocument={async () => {
        await runAction(async () => {
          await saveCurrentDocument();
        });
      }}
      onLoadDocument={async () => {
        const filePath = await pickLoadDocumentPath({
          translate,
          addMessage,
        });
        if (!filePath) {
          return;
        }

        requestUnsavedGate({ kind: "load", filePath });
      }}
      onUndo={async () => {
        await runAction(undo);
      }}
      onRedo={async () => {
        await runAction(redo);
      }}
      logCount={logCount}
      errorLogCount={errorLogCount}
      onOpenLogs={() => {
        setIsLogsOpen(true);
      }}
      onOpenSettings={() => {
        setIsSettingsOpen(true);
      }}
      showAiAssistant={showAiAssistant}
      isAiPanelOpen={isAiPanelOpen}
      onToggleAiPanel={() => {
        setIsAiPanelOpen((current) => !current);
      }}
      onAddBoxFeature={async (width, height, depth) => {
        await runAction(async () => {
          await addBoxFeature(width, height, depth);
        });
      }}
      onAddCylinderFeature={async (radius, height) => {
        await runAction(async () => {
          await addCylinderFeature(radius, height);
        });
      }}
      canExtrude={canExtrudeFromSelection}
      onExtrude={triggerExtrudeAction}
      canLoft={canStartSolidFeatureAction}
      onLoft={triggerLoftAction}
      canRevolve={canStartSolidFeatureAction}
      onRevolve={triggerRevolveAction}
      canSweep={canStartSolidFeatureAction}
      onSweep={triggerSweepAction}
      canHole={canStartSolidFeatureAction}
      onHole={triggerHoleAction}
      canThread={canStartSolidFeatureAction}
      onThread={triggerThreadAction}
      canFastener={canStartSolidFeatureAction}
      onFastener={triggerFastenerAction}
      canEdgeOp={canStartSolidFeatureAction}
      onFillet={async () => {
        await triggerEdgeOpAction("fillet");
      }}
      onChamfer={async () => {
        await triggerEdgeOpAction("chamfer");
      }}
      canMove={canStartSolidFeatureAction}
      onMove={triggerMoveAction}
      canShell={canStartSolidFeatureAction}
      onShell={triggerShellAction}
      canOffsetPlane={canStartReferencePlaneAction}
      onOffsetPlane={() => {
        void triggerOffsetPlaneAction();
      }}
      canMidplane={canStartReferencePlaneAction}
      canTangentPlane={canStartReferencePlaneAction}
      canAnglePlane={canStartReferencePlaneAction}
      canConstructionAxis={canStartConstructionReferenceAction}
      canConstructionPoint={canStartConstructionReferenceAction}
      canHelix={canStartHelixRibbonAction}
      onMidplane={() => {
        void triggerMidplaneAction();
      }}
      onTangentPlane={() => {
        void triggerTangentPlaneAction();
      }}
      onAnglePlane={() => {
        void triggerAnglePlaneAction();
      }}
      onConstructionAxis={() => {
        void triggerConstructionAxisAction();
      }}
      onConstructionPoint={() => {
        void triggerConstructionPointAction();
      }}
      onHelix={() => {
        void triggerHelixAction();
      }}
      onStartSketch={triggerCreateSketchAction}
      onFinishSketch={finishActiveSketch}
      onSetSketchTool={setActiveSketchTool}
      onArmSketchConstraint={async (constraint) => {
        let shouldSwitchToSelect = false;

        setArmedSketchConstraint((current) => {
          const transition = toggleArmedSketchConstraint(current, constraint);
          shouldSwitchToSelect = transition.shouldSwitchToSelect;
          return transition.next;
        });

        if (shouldSwitchToSelect && activeSketchTool !== "select") {
          await runAction(async () => {
            await setSketchTool("select");
          });
        }
      }}
      onCancelSketchConstraint={clearArmedSketchConstraint}
      onWorkspaceDropdownOpenChange={handleWorkspaceDropdownOpenChange}
      parametersPanelOpen={parametersPanelOpen}
      onToggleParametersPanel={() => {
        setParametersPanelOpen((current) => !current);
      }}
      filterPanelOpen={filterPanelOpen}
      onToggleFilterPanel={() => {
        setFilterPanelOpen((current) => !current);
      }}
      materialsPanelOpen={materialsPanelOpen}
      onToggleMaterialsPanel={() => {
        setMaterialsPanelOpen((current) => !current);
      }}
      onUpdateSelectionFilter={updateSelectionFilter}
      activeCamOperation={activeCamOperation}
      onSelectCamOperation={(op) => {
        setActiveCamOperation((prev) => (prev === op ? null : op));
      }}
      hasCamSetup={document?.cam_setup != null}
      onCamSetupClick={() => {
        setIsCamSetupPanelOpen((prev) => !prev);
      }}
      onCamFaceMillingClick={() => {
        const body = viewport?.bodies?.[0];
        if (!body) {
          return;
        }
        void runAction(async () => {
          await camFaceMillingCreate(body.id, 0);
        });
      }}
    />
  );
}
