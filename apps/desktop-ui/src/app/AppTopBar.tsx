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
  pickExportDxfPath,
  pickExportPath,
  pickImportDxfPath,
  pickImportStepPath,
  pickImportStlPath,
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
  showDrawingView: AsyncVoid;
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
  dimensionToolMode: import("@/types").DimensionToolMode;
  onSetDimensionToolMode: (mode: import("@/types").DimensionToolMode) => void;
  bodyProjectionMode: "section" | "silhouette";
  setBodyProjectionMode: (mode: "section" | "silhouette") => void;
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
  exportDocumentDxf: (filePath: string) => Promise<void>;
  importStl: (filePath: string, scale?: number) => Promise<void>;
  importDxf: (filePath: string, planeId?: string) => Promise<void>;
  importStep: (filePath: string) => Promise<void>;
  saveCurrentDocument: () => Promise<unknown>;
  undo: AsyncVoid;
  redo: AsyncVoid;
  pluginMenuItems: Array<{
    id: string;
    pluginId: string;
    label: string;
    command: string;
    disabled?: boolean;
    disabledWhenCoreOffline?: boolean;
  }>;
  onPluginCommand: (pluginId: string, command: string) => void;
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
  viewPanelOpen: boolean;
  setViewPanelOpen: Dispatch<SetStateAction<boolean>>;
  updateSelectionFilter: (filter: SelectionFilterUpdate) => Promise<void>;
  activeCamOperation: CamOperationType | null;
  setActiveCamOperation: Dispatch<SetStateAction<CamOperationType | null>>;
  setIsCamSetupPanelOpen: Dispatch<SetStateAction<boolean>>;
  camFaceMillingCreate: (
    bodyId: string,
    faceIndex: number,
  ) => Promise<void>;
}

export function AppTopBar(props: AppTopBarProps) {
  return (
    <AppHeader
      workspaceView={props.workspaceView}
      canOpenSlicerView={props.canOpenSlicerView}
      canExportToSlicer={props.canExportToSlicer}
      onSetWorkspaceView={(view) => {
        if (view === "cad") {
          void props.showCadView();
          return;
        }
        if (view === "cam") {
          void props.showCamView();
          return;
        }
        if (view === "drawing") {
          void props.showDrawingView();
          return;
        }
        void props.showSlicerView();
      }}
      onExportToSlicer={() => void props.exportToSlicer()}
      status={props.status}
      disabled={props.status !== "connected"}
      canUndo={props.canUndo}
      canRedo={props.canRedo}
      activeSketchPlaneId={props.activeSketchPlaneId}
      activeSketchTool={props.activeSketchTool}
      selectedReferenceId={props.selectedReferenceId}
      selectedFaceId={props.selectedFaceId}
      armedSketchConstraint={props.armedSketchConstraint}
      isMirrorToolOpen={props.isMirrorToolOpen}
      arcToolMode={props.arcToolMode}
      onSetArcToolMode={props.setArcToolMode}
      rectangleToolMode={props.rectangleToolMode}
      onSetRectangleToolMode={props.setRectangleToolMode}
      circleToolMode={props.circleToolMode}
      onSetCircleToolMode={props.setCircleToolMode}
      polygonToolMode={props.polygonToolMode}
      onSetPolygonToolMode={props.setPolygonToolMode}
      dimensionToolMode={props.dimensionToolMode}
      onSetDimensionToolMode={props.onSetDimensionToolMode}
      bodyProjectionMode={props.bodyProjectionMode}
      onSetBodyProjectionMode={props.setBodyProjectionMode}
      onStart={async () => {
        await props.runAction(props.start);
      }}
      onStartMirrorTool={async () => {
        await props.runAction(async () => {
          await props.startMirrorPreview();
          props.setMirrorFocusedSlot("objects");
          props.clearArmedSketchConstraint();
        });
      }}
      onCreateDocument={async () => {
        props.requestUnsavedGate({ kind: "new" });
      }}
      onExportDocument={async () => {
        const filePath = await pickExportPath({
          translate: props.translate,
          documentName: props.document?.name,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        await props.runAction(async () => {
          await props.exportDocument(filePath);
          props.addMessage(`export requested: ${filePath}`);
        });
      }}
      onSaveDocument={async () => {
        await props.runAction(async () => {
          await props.saveCurrentDocument();
        });
      }}
      onLoadDocument={async () => {
        const filePath = await pickLoadDocumentPath({
          translate: props.translate,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        props.requestUnsavedGate({ kind: "load", filePath });
      }}
      onImportMesh={async () => {
        const filePath = await pickImportStlPath({
          translate: props.translate,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        await props.runAction(async () => {
          await props.importStl(filePath, 1.0);
          props.addMessage(`import requested: ${filePath}`);
        });
      }}
      onImportDxf={async () => {
        const filePath = await pickImportDxfPath({
          translate: props.translate,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        await props.runAction(async () => {
          await props.importDxf(filePath);
          props.addMessage(`import requested: ${filePath}`);
        });
      }}
      onImportStep={async () => {
        const filePath = await pickImportStepPath({
          translate: props.translate,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        await props.runAction(async () => {
          await props.importStep(filePath);
          props.addMessage(`import requested: ${filePath}`);
        });
      }}
      onExportDxf={async () => {
        const filePath = await pickExportDxfPath({
          translate: props.translate,
          documentName: props.document?.name,
          addMessage: props.addMessage,
        });
        if (!filePath) {
          return;
        }

        await props.runAction(async () => {
          await props.exportDocumentDxf(filePath);
          props.addMessage(`export requested: ${filePath}`);
        });
      }}
      onUndo={async () => {
        await props.runAction(props.undo);
      }}
      onRedo={async () => {
        await props.runAction(props.redo);
      }}
      pluginMenuItems={props.pluginMenuItems}
      onPluginCommand={props.onPluginCommand}
      logCount={props.logCount}
      errorLogCount={props.errorLogCount}
      onOpenLogs={() => {
        props.setIsLogsOpen(true);
      }}
      onOpenSettings={() => {
        props.setIsSettingsOpen(true);
      }}
      showAiAssistant={props.showAiAssistant}
      isAiPanelOpen={props.isAiPanelOpen}
      onToggleAiPanel={() => {
        props.setIsAiPanelOpen((current) => !current);
      }}
      onAddBoxFeature={async (width, height, depth) => {
        await props.runAction(async () => {
          await props.addBoxFeature(width, height, depth);
        });
      }}
      onAddCylinderFeature={async (radius, height) => {
        await props.runAction(async () => {
          await props.addCylinderFeature(radius, height);
        });
      }}
      canExtrude={props.canExtrudeFromSelection}
      onExtrude={props.triggerExtrudeAction}
      canLoft={props.canStartSolidFeatureAction}
      onLoft={props.triggerLoftAction}
      canRevolve={props.canStartSolidFeatureAction}
      onRevolve={props.triggerRevolveAction}
      canSweep={props.canStartSolidFeatureAction}
      onSweep={props.triggerSweepAction}
      canHole={props.canStartSolidFeatureAction}
      onHole={props.triggerHoleAction}
      canThread={props.canStartSolidFeatureAction}
      onThread={props.triggerThreadAction}
      canFastener={props.canStartSolidFeatureAction}
      onFastener={props.triggerFastenerAction}
      canEdgeOp={props.canStartSolidFeatureAction}
      onFillet={async () => {
        await props.triggerEdgeOpAction("fillet");
      }}
      onChamfer={async () => {
        await props.triggerEdgeOpAction("chamfer");
      }}
      canMove={props.canStartSolidFeatureAction}
      onMove={props.triggerMoveAction}
      canShell={props.canStartSolidFeatureAction}
      onShell={props.triggerShellAction}
      canOffsetPlane={props.canStartReferencePlaneAction}
      onOffsetPlane={() => {
        void props.triggerOffsetPlaneAction();
      }}
      canMidplane={props.canStartReferencePlaneAction}
      canTangentPlane={props.canStartReferencePlaneAction}
      canAnglePlane={props.canStartReferencePlaneAction}
      canConstructionAxis={props.canStartConstructionReferenceAction}
      canConstructionPoint={props.canStartConstructionReferenceAction}
      canHelix={props.canStartHelixRibbonAction}
      onMidplane={() => {
        void props.triggerMidplaneAction();
      }}
      onTangentPlane={() => {
        void props.triggerTangentPlaneAction();
      }}
      onAnglePlane={() => {
        void props.triggerAnglePlaneAction();
      }}
      onConstructionAxis={() => {
        void props.triggerConstructionAxisAction();
      }}
      onConstructionPoint={() => {
        void props.triggerConstructionPointAction();
      }}
      onHelix={() => {
        void props.triggerHelixAction();
      }}
      onStartSketch={props.triggerCreateSketchAction}
      onFinishSketch={props.finishActiveSketch}
      onSetSketchTool={props.setActiveSketchTool}
      onArmSketchConstraint={async (constraint) => {
        let shouldSwitchToSelect = false;

        props.setArmedSketchConstraint((current) => {
          const transition = toggleArmedSketchConstraint(current, constraint);
          shouldSwitchToSelect = transition.shouldSwitchToSelect;
          return transition.next;
        });

        if (shouldSwitchToSelect && props.activeSketchTool !== "select") {
          await props.runAction(async () => {
            await props.setSketchTool("select");
          });
        }
      }}
      onCancelSketchConstraint={props.clearArmedSketchConstraint}
      onWorkspaceDropdownOpenChange={props.handleWorkspaceDropdownOpenChange}
      parametersPanelOpen={props.parametersPanelOpen}
      onToggleParametersPanel={() => {
        props.setParametersPanelOpen((current) => !current);
      }}
      filterPanelOpen={props.filterPanelOpen}
      onToggleFilterPanel={() => {
        props.setFilterPanelOpen((current) => !current);
      }}
      materialsPanelOpen={props.materialsPanelOpen}
      onToggleMaterialsPanel={() => {
        props.setMaterialsPanelOpen((current) => !current);
      }}
      viewPanelOpen={props.viewPanelOpen}
      onToggleViewPanel={() => {
        props.setViewPanelOpen((current) => !current);
      }}
      onUpdateSelectionFilter={props.updateSelectionFilter}
      activeCamOperation={props.activeCamOperation}
      onSelectCamOperation={(op) => {
        props.setActiveCamOperation((prev) => (prev === op ? null : op));
      }}
      hasCamSetup={(props.document?.cam as any)?.setups?.length > 0}
      onCamSetupClick={() => {
        props.setIsCamSetupPanelOpen((prev) => !prev);
      }}
      onCamFaceMillingClick={() => {
        const body = props.viewport?.bodies?.[0];
        if (!body) {
          return;
        }
        void props.runAction(async () => {
          await props.camFaceMillingCreate(body.id, 0);
        });
      }}
    />
  );
}
