import type {
  FocusEvent,
  KeyboardEvent as ReactKeyboardEvent,
  MutableRefObject,
  RefObject,
} from "react";

import { formatHotkey } from "@/config";
import type {
  ArmedSketchConstraint,
  DocumentState,
  SketchDimensionScene,
  SketchTool,
  ViewportState,
} from "@/types";
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type { SelectedConstraintState } from "./contextMenuState";
import type { ConstraintPreviewState } from "./constraintPreview";
import type {
  DraftDimensionField,
  DraftDimensionSession,
  ParameterSuggestion,
} from "./draftDimensions";
import type { DraftSuggestionState } from "./draftDimensionInput";
import type { SelectionRectOverlay } from "./selectionGeometry";
import { SketchToolPanel } from "./SketchToolPanel";
import {
  ConstraintPreviewBadge,
  CrosshairGuideOverlay,
  DimensionToolHint,
  GridToggleToolbar,
  SelectionRectangleOverlay,
  ViewportBlockingOverlay,
  ViewportStatusPanel,
} from "./ViewportOverlays";
import { ViewportContextMenu } from "./ViewportContextMenu";
import {
  DimensionEditorOverlay,
  DraftDimensionFieldEditors,
} from "./ViewportDimensionEditors";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import type { PolygonToolMode } from "./viewportPanelTypes";

type ViewportTranslate = (
  key: string,
  options?: Record<string, unknown>,
) => string;
type HotkeyBinding = Parameters<typeof formatHotkey>[0];

interface ViewportContextMenuActions {
  getCircleDimensionToggleLabel: (dimensionId: string | null) => string;
  isLinkedBodyCopy: (featureId: string | null) => boolean;
  toggleDimensionDisplay: () => void | Promise<void>;
  deleteDimension: () => void | Promise<void>;
  deleteConstraint: () => void | Promise<void>;
  deleteSketchSelection: () => void | Promise<void>;
  moveBody: () => void | Promise<void>;
  copyBody: (copyMode: "linked" | "standalone") => void | Promise<void>;
  unlinkBodyCopy: () => void | Promise<void>;
  exportBodyMesh: () => void | Promise<void>;
  createSketch: () => void | Promise<void>;
}

interface ViewportPanelShellProps {
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool;
  arcCount: number;
  arcToolMode: ArcToolMode;
  armedSketchConstraint: ArmedSketchConstraint;
  canvasRef: RefObject<HTMLCanvasElement | null>;
  circleCount: number;
  circleToolMode: CircleToolMode;
  constraintPreview: ConstraintPreviewState | null;
  contextMenu: import("@/types").ViewportContextMenuState | null;
  contextMenuActions: ViewportContextMenuActions;
  crosshairCanvasClass: string;
  crosshairGuideSize: number;
  crosshairPointer: { x: number; y: number } | null;
  dimensionDraftValue: string;
  dimensionEditorRef: RefObject<HTMLFormElement | null>;
  dimensionInputRef: RefObject<HTMLInputElement | null>;
  dimensionParameterSuggestions: ParameterSuggestion[];
  dimensionSuggestionIndex: number;
  dimensionToolHotkey: HotkeyBinding;
  dimensionToolFirstLine: string | null;
  document: DocumentState | null;
  draftDimensionInputRefs: MutableRefObject<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >;
  draftDimensionSession: DraftDimensionSession | null;
  draftSuggestionState: DraftSuggestionState;
  finishDisabled: boolean;
  hasActiveDocument: boolean;
  hostRef: RefObject<HTMLDivElement | null>;
  isDimensionEditorOpen: boolean;
  isSketchDrawingCursor: boolean;
  isSketchMode: boolean;
  lineCount: number;
  lineDraftActive: boolean;
  measurementText: string | null;
  pointCount: number;
  polygonSides: number;
  polygonToolMode: PolygonToolMode;
  rectangleToolMode: RectangleToolMode;
  selectedConstraint: SelectedConstraintState | null;
  selectedEntityDof: ViewportState["dof_statuses"][number] | null;
  selectedPrimitiveLabel: string | null;
  selectedReference: { label: string } | null;
  selectedSketchDimension: SketchDimensionScene | null;
  selectionRect: SelectionRectOverlay | null;
  showSketchGrid: boolean;
  showViewportGrid: boolean;
  sketchSnapLabel: string | null;
  sketchToolConstruction: boolean;
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  translate: ViewportTranslate;
  usesCrosshairGuide: boolean;
  viewportGridHotkey: HotkeyBinding;
  onCommitDraftDimensionSession: () => void | Promise<void>;
  onDraftDimensionBlur: (field: DraftDimensionField) => void;
  onDraftDimensionChange: (field: DraftDimensionField, value: string) => void;
  onDraftDimensionFocus: (field: DraftDimensionField) => void;
  onDraftDimensionKeyDown: (
    event: ReactKeyboardEvent<HTMLInputElement>,
    field: DraftDimensionField,
  ) => void;
  onDimensionDraftChange: (value: string) => void;
  onDimensionEditorFocus: (event: FocusEvent<HTMLInputElement>) => void;
  onDimensionEditorKeyDown: (
    event: ReactKeyboardEvent<HTMLInputElement>,
  ) => void;
  onFinishSketch: () => void;
  onInsertDimensionParameterSuggestion: (name: string) => void;
  onInsertDraftParameterSuggestion: (
    field: DraftDimensionField,
    name: string,
  ) => void;
  onPolygonSidesChange: (sides: number) => void;
  onSetArcToolMode: (mode: ArcToolMode) => void;
  onSetCircleToolMode: (mode: CircleToolMode) => void;
  onSetPolygonToolMode: (mode: PolygonToolMode) => void;
  onSetRectangleToolMode: (mode: RectangleToolMode) => void;
  onSketchToolConstructionChange: (checked: boolean) => void;
  onSubmitDimensionEdit: () => void | Promise<void>;
  onToggleGrid: () => void;
  getDraftFieldInputValue: (
    session: DraftDimensionSession,
    field: DraftDimensionField,
  ) => string;
  getDraftParameterSuggestions: (
    field: DraftDimensionField,
    inputValue: string,
  ) => ParameterSuggestion[];
  getDraftScreenPosition: (
    field: DraftDimensionField,
  ) => { x: number; y: number } | null;
}

export function ViewportPanelShell({
  activeSketchPlaneId,
  activeSketchTool,
  arcCount,
  arcToolMode,
  armedSketchConstraint,
  canvasRef,
  circleCount,
  circleToolMode,
  constraintPreview,
  contextMenu,
  contextMenuActions,
  crosshairCanvasClass,
  crosshairGuideSize,
  crosshairPointer,
  dimensionDraftValue,
  dimensionEditorRef,
  dimensionInputRef,
  dimensionParameterSuggestions,
  dimensionSuggestionIndex,
  dimensionToolHotkey,
  dimensionToolFirstLine,
  document,
  draftDimensionInputRefs,
  draftDimensionSession,
  draftSuggestionState,
  finishDisabled,
  hasActiveDocument,
  hostRef,
  isDimensionEditorOpen,
  isSketchDrawingCursor,
  isSketchMode,
  lineCount,
  lineDraftActive,
  measurementText,
  pointCount,
  polygonSides,
  polygonToolMode,
  rectangleToolMode,
  selectedConstraint,
  selectedEntityDof,
  selectedPrimitiveLabel,
  selectedReference,
  selectedSketchDimension,
  selectionRect,
  showSketchGrid,
  showViewportGrid,
  sketchSnapLabel,
  sketchToolConstruction,
  status,
  translate,
  usesCrosshairGuide,
  viewportGridHotkey,
  onCommitDraftDimensionSession,
  onDraftDimensionBlur,
  onDraftDimensionChange,
  onDraftDimensionFocus,
  onDraftDimensionKeyDown,
  onDimensionDraftChange,
  onDimensionEditorFocus,
  onDimensionEditorKeyDown,
  onFinishSketch,
  onInsertDimensionParameterSuggestion,
  onInsertDraftParameterSuggestion,
  onPolygonSidesChange,
  onSetArcToolMode,
  onSetCircleToolMode,
  onSetPolygonToolMode,
  onSetRectangleToolMode,
  onSketchToolConstructionChange,
  onSubmitDimensionEdit,
  onToggleGrid,
  getDraftFieldInputValue,
  getDraftParameterSuggestions,
  getDraftScreenPosition,
}: ViewportPanelShellProps) {
  return (
    <section className="relative flex h-full min-h-0 flex-col overflow-hidden">
      {showViewportGrid && !activeSketchPlaneId ? (
        <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      ) : null}
      <div
        ref={hostRef}
        className="absolute inset-0 min-h-0 min-w-0 overflow-hidden rounded-[18px]"
      >
        {contextMenu ? (
          <ViewportContextMenu
            contextMenu={contextMenu}
            translate={translate}
            getCircleDimensionToggleLabel={
              contextMenuActions.getCircleDimensionToggleLabel
            }
            isLinkedBodyCopy={contextMenuActions.isLinkedBodyCopy}
            onToggleDimensionDisplay={contextMenuActions.toggleDimensionDisplay}
            onDeleteDimension={contextMenuActions.deleteDimension}
            onDeleteConstraint={contextMenuActions.deleteConstraint}
            onDeleteSketchSelection={contextMenuActions.deleteSketchSelection}
            onMoveBody={contextMenuActions.moveBody}
            onCopyBody={contextMenuActions.copyBody}
            onUnlinkBodyCopy={contextMenuActions.unlinkBodyCopy}
            onExportBodyMesh={contextMenuActions.exportBodyMesh}
            onCreateSketch={contextMenuActions.createSketch}
          />
        ) : null}
        <canvas
          ref={canvasRef}
          className={`cad-viewport-canvas absolute inset-0 h-full w-full ${crosshairCanvasClass}`}
        />
        <GridToggleToolbar
          active={isSketchMode ? showSketchGrid : showViewportGrid}
          label={`${
            isSketchMode
              ? showSketchGrid
                ? translate("viewport.hideSketchGrid")
                : translate("viewport.showSketchGrid")
              : showViewportGrid
                ? translate("viewport.hideViewportGrid")
                : translate("viewport.showViewportGrid")
          } (${formatHotkey(viewportGridHotkey)})`}
          onToggle={onToggleGrid}
        />
        <CrosshairGuideOverlay
          pointer={crosshairPointer}
          size={crosshairGuideSize}
          visible={isSketchDrawingCursor}
        />
        <SelectionRectangleOverlay overlay={selectionRect} />
        <ConstraintPreviewBadge preview={constraintPreview} />
        <DraftDimensionFieldEditors
          session={draftDimensionSession}
          suggestionState={draftSuggestionState}
          inputRefs={draftDimensionInputRefs}
          getScreenPosition={getDraftScreenPosition}
          getInputValue={getDraftFieldInputValue}
          getSuggestions={getDraftParameterSuggestions}
          onSubmit={() => {
            void onCommitDraftDimensionSession();
          }}
          onChange={onDraftDimensionChange}
          onFocus={onDraftDimensionFocus}
          onBlur={onDraftDimensionBlur}
          onKeyDown={onDraftDimensionKeyDown}
          onInsertSuggestion={onInsertDraftParameterSuggestion}
        />
        {activeSketchPlaneId ? (
          <SketchToolPanel
            translate={translate}
            activeSketchTool={activeSketchTool}
            sketchToolConstruction={sketchToolConstruction}
            arcToolMode={arcToolMode}
            circleToolMode={circleToolMode}
            rectangleToolMode={rectangleToolMode}
            polygonToolMode={polygonToolMode}
            polygonSides={polygonSides}
            onConstructionChange={onSketchToolConstructionChange}
            onSetArcToolMode={onSetArcToolMode}
            onSetCircleToolMode={onSetCircleToolMode}
            onSetRectangleToolMode={onSetRectangleToolMode}
            onSetPolygonToolMode={onSetPolygonToolMode}
            onPolygonSidesChange={onPolygonSidesChange}
          />
        ) : null}
        <DimensionToolHint
          hotkeyLabel={formatHotkey(dimensionToolHotkey)}
          placeLabel={translate("viewport.placeDimension")}
          readyLabel={translate("viewport.dimensionReady")}
          title={translate("viewport.dimensionTool")}
          waitingForFirstLine={dimensionToolFirstLine === null}
          visible={Boolean(activeSketchPlaneId) && activeSketchTool === "dimension"}
        />
        <DimensionEditorOverlay
          visible={Boolean(
            selectedSketchDimension &&
              activeSketchPlaneId &&
              isDimensionEditorOpen,
          )}
          editorRef={dimensionEditorRef}
          inputRef={dimensionInputRef}
          draftValue={dimensionDraftValue}
          suggestions={dimensionParameterSuggestions}
          suggestionIndex={dimensionSuggestionIndex}
          onSubmit={() => {
            void onSubmitDimensionEdit();
          }}
          onDraftChange={onDimensionDraftChange}
          onFocus={onDimensionEditorFocus}
          onKeyDown={onDimensionEditorKeyDown}
          onInsertSuggestion={onInsertDimensionParameterSuggestion}
        />
        <ViewportBlockingOverlay
          kicker={translate("viewport.title")}
          message={translate("viewport.noActiveDocument")}
          variant="empty"
          visible={!hasActiveDocument}
        />
        <ViewportBlockingOverlay
          kicker={translate("viewport.coreStartup")}
          message={translate("viewport.startingCore")}
          variant="starting"
          visible={status === "starting"}
        />
        <ViewportStatusPanel
          activeSketchPlaneId={activeSketchPlaneId}
          activeSketchTool={activeSketchTool}
          arcCount={arcCount}
          armedSketchConstraint={armedSketchConstraint}
          circleCount={circleCount}
          document={document}
          finishDisabled={finishDisabled}
          lineCount={lineCount}
          lineDraftActive={lineDraftActive}
          measurementText={measurementText}
          onFinishSketch={onFinishSketch}
          pointCount={pointCount}
          selectedConstraint={selectedConstraint}
          selectedEntityDof={selectedEntityDof}
          selectedPrimitiveLabel={selectedPrimitiveLabel}
          selectedReference={selectedReference}
          sketchSnapLabel={sketchSnapLabel}
          translate={translate}
          visible={hasActiveDocument}
        />
      </div>
    </section>
  );
}
