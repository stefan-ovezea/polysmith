import {
  sendCoreCommand,
  startCadCore,
  makeCreateDocumentCommand,
  makeCamSetupCreateCommand,
  makeCamSetupUpdateCommand,
  makeCamOperationCreateCommand,
  makeCamOperationUpdateCommand,
  makeCamOperationDeleteCommand,
  makeAddBoxFeatureCommand,
  makeAddCylinderFeatureCommand,
  makeAddSketchArcCommand,
  makeAddSketchFilletCommand,
  makeUpdateSketchFilletRadiusCommand,
  makeDeleteSketchFilletCommand,
  makeDeleteSketchDimensionCommand,
  makeToggleSketchDimensionDrivenCommand,
  makeTrimSketchEntityCommand,
  makeUpdateSketchDimensionDisplayCommand,
  makeUpdateSketchDimensionLabelPositionCommand,
  makeAddParameterCommand,
  makeUpdateParameterCommand,
  makeDeleteParameterCommand,
  makeUpdateSelectionFilterCommand,
  makeDeleteSketchSelectionCommand,
  makeAddSketchCircleCommand,
  makeAddSketchPolygonCommand,
  makeAddSketchLineCommand,
  makeSetSketchLineConstructionCommand,
  makeSetSketchMidpointAnchorCommand,
  makeSetSketchVertexLineAnchorCommand,
  makeAddSketchAngleDimensionCommand,
  makeAddSketchDistanceDimensionCommand,
  makeAddSketchLineLengthDimensionCommand,
  makeAddSketchLineAngleDimensionCommand,
  makeAddSketchArcRadiusDimensionCommand,
  makeAddSketchCircleRadiusDimensionCommand,
  makeAddSketchVertexDistanceDimensionCommand,
  makeAddSketchPolygonRadiusDimensionCommand,
  makeAddSketchRectangleCommand,
  makeClearSelectionCommand,
  makeDeleteFeatureCommand,
  makeExportBodyStlCommand,
  makeExportDocumentCommand,
  makeExportDocumentStlCommand,
  makeExportDocumentDxfCommand,
  makeImportStlCommand,
  makeImportDxfCommand,
  makeConvertMeshToBodyCommand,
  makeDetachBodyProjectionsCommand,
  makeLoadDocumentCommand,
  makeProjectEdgeIntoSketchCommand,
  makeProjectFaceIntoSketchCommand,
  makeProjectProfileIntoSketchCommand,
  makeProjectVertexIntoSketchCommand,
  makeProjectBodyIntoSketchCommand,
  makeSaveDocumentCommand,
  makeFinishSketchCommand,
  makeReenterSketchCommand,
  makeGetDocumentStateCommand,
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  makePingCommand,
  makeRedoCommand,
  makeSelectSketchProfileCommand,
  makeSelectSketchDimensionCommand,
  makeSelectSketchEntityCommand,
  makeSelectSketchVertexCommand,
  makeSetTimelineCursorCommand,
  makeRenameFeatureCommand,
  makeSetFeatureSuppressedCommand,
  makeSelectFeatureCommand,
  makeSelectReferenceCommand,
  makeSelectFaceCommand,
  makeSelectEdgeCommand,
  makeSelectVertexCommand,
  makeSetBodyColorCommand,
  makeSetFaceColorCommand,
  makeClearBodyColorCommand,
  makeClearFaceColorCommand,
  makeClearAppearanceOverridesCommand,
  makeCreateFilletCommand,
  makeUpdateFilletEdgesCommand,
  makeUpdateFilletRadiusCommand,
  makeUpdateChamferEdgesCommand,
  makeCreateChamferCommand,
  makeUpdateChamferDistanceCommand,
  makeConfirmFilletCommand,
  makeConfirmChamferCommand,
  makeCreateShellCommand,
  makeUpdateShellThicknessCommand,
  makeConfirmShellCommand,
  makeCreateOffsetPlaneCommand,
  makeCreateMidplaneCommand,
  makeCreateTangentPlaneCommand,
  makeCreateAnglePlaneCommand,
  makeCreateConstructionAxisCommand,
  makeCreateConstructionPointCommand,
  makeCreateFastenerCommand,
  makeCreateHelixCommand,
  makeCreateHoleCommand,
  makeCreateBodyCopyCommand,
  makeCreateMoveCommand,
  makeCreateThreadCommand,
  makeConfirmHoleCommand,
  makeConfirmMoveCommand,
  makeConfirmThreadCommand,
  makeUpdateFastenerParametersCommand,
  makeUpdateHelixParametersCommand,
  makeUpdateHoleParametersCommand,
  makeUpdateMoveParametersCommand,
  makeUnlinkBodyCopyCommand,
  makeUpdateThreadParametersCommand,
  makeUpdateOffsetPlaneCommand,
  makeUpdateAnglePlaneCommand,
  makeSetSketchCoincidentConstraintCommand,
  makeDeleteSketchCoincidentConstraintCommand,
  makeSetSketchEqualLengthConstraintCommand,
  makeSetSketchParallelConstraintCommand,
  makeSetSketchPerpendicularConstraintCommand,
  makeSetSketchTangentConstraintCommand,
  makeStartMirrorPreviewCommand,
  makeUpdateMirrorPreviewAxisCommand,
  makeUpdateMirrorPreviewObjectsCommand,
  makeCommitMirrorPreviewCommand,
  makeCancelMirrorPreviewCommand,
  makeSetSketchVertexFixedCommand,
  makeExtrudeFaceCommand,
  makeExtrudeOpenEntitiesCommand,
  makeExtrudeProfileCommand,
  makeSetSketchLineConstraintCommand,
  makeClearSketchLineConstraintsCommand,
  makeSetSketchToolCommand,
  makeStartSketchOnPlaneCommand,
  makeStartSketchOnFaceCommand,
  makeUndoCommand,
  makeUpdateSketchCircleCommand,
  makeUpdateSketchDimensionCommand,
  makeUpdateSketchLineCommand,
  makeUpdateSketchVertexCommand,
  makeMoveSketchEntitiesCommand,
  makeUpdateBoxFeatureCommand,
  makeUpdateCylinderFeatureCommand,
  makeUpdateExtrudeDepthCommand,
  makeUpdateExtrudeModeCommand,
  makeUpdateExtrudeParametersCommand,
  makeUpdateExtrudeProfilesCommand,
  makeUpdateExtrudeTargetBodyCommand,
  makeLoftProfilesCommand,
  makeRevolveProfileCommand,
  makeUpdateRevolveAngleCommand,
  makeUpdateRevolveAxisCommand,
  makeUpdateRevolveProfileCommand,
  makeSweepProfileCommand,
  makeUpdateSweepPathCommand,
  makeUpdateSweepProfileCommand,
  makeUpdateLoftProfilesCommand,
  makeUpdateLoftRuledCommand,
  makeUiLogEntry,
  writeLogToConsole,
} from "@/lib";
import type {
  ExtrudeAdvancedParameters,
  ExtrudeFeatureParameters,
  ExtrudeMode,
  FastenerFeatureParameters,
  HelixFeatureParameters,
  HoleFeatureParameters,
  MoveFeatureParameters,
  SelectionFilterUpdate,
  ThreadFeatureParameters,
} from "@/types";

import { useCadCoreStore } from "@/state";
import { SketchTool } from "@/types";
import {
  sendAndRefreshSessionViewport,
  sendAndRefreshViewport,
} from "./cadCoreCommandRefresh";
import { reportCoreError } from "./coreLogReporting";
import { useCadCoreEventBridge } from "./useCadCoreEventBridge";
import type { SketchPlaneFramePayload } from "@/lib/ipc/sketchCommands";

export function useCadCore() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
  const setStatus = useCadCoreStore((state) => state.setStatus);

  useCadCoreEventBridge();

  return {
    start: async () => {
      setStatus("starting");
      try {
        const result = await startCadCore();
        const entry = makeUiLogEntry("info", "desktop_ui", `start: ${result}`);
        writeLogToConsole(entry);
        addLogEntry(entry);
        addMessage(`start: ${result}`);
      } catch (error) {
        reportCoreError(
          { addLogEntry, addMessage, setStatus },
          "desktop_ui",
          `start failed: ${String(error)}`,
        );
      }
    },
    ping: async () => {
      await sendCoreCommand(makePingCommand());
    },
    createDocument: async () => {
      await sendAndRefreshSessionViewport(makeCreateDocumentCommand());
    },
    refreshDocument: async () => {
      await sendCoreCommand(makeGetDocumentStateCommand());
    },
    refreshSession: async () => {
      await sendCoreCommand(makeGetSessionStateCommand());
    },
    refreshViewport: async () => {
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    exportDocument: async (filePath: string) => {
      await sendCoreCommand(makeExportDocumentCommand(filePath));
    },
    exportDocumentStl: async (filePath: string) => {
      await sendCoreCommand(makeExportDocumentStlCommand(filePath));
    },
    exportDocumentDxf: async (filePath: string) => {
      await sendCoreCommand(makeExportDocumentDxfCommand(filePath));
    },
    exportBodyStl: async (filePath: string, bodyId: string) => {
      await sendCoreCommand(makeExportBodyStlCommand(filePath, bodyId));
    },
    saveDocument: async (filePath: string) => {
      await sendCoreCommand(makeSaveDocumentCommand(filePath));
    },
    loadDocument: async (filePath: string) => {
      await sendCoreCommand(makeLoadDocumentCommand(filePath));
      // The load command replies with `document_state`. Refresh session
      // (undo/redo flags) and viewport so the UI reflects the loaded
      // document end-to-end.
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    importStl: async (filePath: string, scale = 1.0) => {
      await sendAndRefreshSessionViewport(
        makeImportStlCommand(filePath, scale),
      );
    },
    importDxf: async (filePath: string, planeId?: string) => {
      await sendAndRefreshSessionViewport(
        makeImportDxfCommand(filePath, planeId),
      );
    },
    convertMeshToBody: async (bodyId: string) => {
      await sendAndRefreshSessionViewport(
        makeConvertMeshToBodyCommand(bodyId),
      );
    },
    detachBodyProjections: async (bodyId: string) => {
      await sendAndRefreshSessionViewport(
        makeDetachBodyProjectionsCommand(bodyId),
      );
    },
    projectBodyIntoSketch: async (
      bodyId: string,
      mode: "section" | "silhouette",
    ) => {
      await sendAndRefreshSessionViewport(
        makeProjectBodyIntoSketchCommand(bodyId, mode),
      );
    },
    projectFaceIntoSketch: async (faceId: string) => {
      await sendAndRefreshSessionViewport(
        makeProjectFaceIntoSketchCommand(faceId),
      );
    },
    projectProfileIntoSketch: async (profileId: string) => {
      await sendAndRefreshSessionViewport(
        makeProjectProfileIntoSketchCommand(profileId),
      );
    },
    projectEdgeIntoSketch: async (edgeId: string) => {
      await sendAndRefreshSessionViewport(
        makeProjectEdgeIntoSketchCommand(edgeId),
      );
    },
    projectVertexIntoSketch: async (vertexId: string) => {
      await sendAndRefreshSessionViewport(
        makeProjectVertexIntoSketchCommand(vertexId),
      );
    },
    addBoxFeature: async (width: number, height: number, depth: number) => {
      await sendAndRefreshSessionViewport(
        makeAddBoxFeatureCommand(width, height, depth),
      );
    },
    addCylinderFeature: async (radius: number, height: number) => {
      await sendAndRefreshSessionViewport(
        makeAddCylinderFeatureCommand(radius, height),
      );
    },
    updateBoxFeature: async (
      featureId: string,
      width: number,
      height: number,
      depth: number,
    ) => {
      await sendAndRefreshSessionViewport(
        makeUpdateBoxFeatureCommand(featureId, width, height, depth),
      );
    },
    updateCylinderFeature: async (
      featureId: string,
      radius: number,
      height: number,
    ) => {
      await sendAndRefreshSessionViewport(
        makeUpdateCylinderFeatureCommand(featureId, radius, height),
      );
    },
    updateExtrudeDepth: async (featureId: string, depth: number) => {
      await sendAndRefreshSessionViewport(
        makeUpdateExtrudeDepthCommand(featureId, depth),
      );
    },
    renameFeature: async (featureId: string, name: string) => {
      await sendAndRefreshSessionViewport(
        makeRenameFeatureCommand(featureId, name),
      );
    },
    setFeatureSuppressed: async (featureId: string, suppressed: boolean) => {
      await sendAndRefreshSessionViewport(
        makeSetFeatureSuppressedCommand(featureId, suppressed),
      );
    },
    deleteFeature: async (featureId: string) => {
      await sendAndRefreshSessionViewport(makeDeleteFeatureCommand(featureId));
    },
    undo: async () => {
      await sendAndRefreshSessionViewport(makeUndoCommand());
    },
    redo: async () => {
      await sendAndRefreshSessionViewport(makeRedoCommand());
    },
    setTimelineCursor: async (includedActionCount: number) => {
      await sendAndRefreshViewport(
        makeSetTimelineCursorCommand(includedActionCount),
      );
    },
    selectFeature: async (featureId: string) => {
      await sendAndRefreshViewport(makeSelectFeatureCommand(featureId));
    },
    selectReference: async (referenceId: string) => {
      await sendAndRefreshViewport(makeSelectReferenceCommand(referenceId));
    },
    selectFace: async (faceId: string) => {
      await sendAndRefreshViewport(makeSelectFaceCommand(faceId));
    },
    selectEdge: async (edgeId: string, additive: boolean = false) => {
      await sendAndRefreshViewport(makeSelectEdgeCommand(edgeId, additive));
    },
    selectVertex: async (vertexId: string, additive: boolean = false) => {
      await sendAndRefreshViewport(makeSelectVertexCommand(vertexId, additive));
    },
    setBodyColor: async (bodyId: string, color: string) => {
      await sendAndRefreshSessionViewport(makeSetBodyColorCommand(bodyId, color));
    },
    setFaceColor: async (faceId: string, color: string) => {
      await sendAndRefreshSessionViewport(makeSetFaceColorCommand(faceId, color));
    },
    clearBodyColor: async (bodyId: string) => {
      await sendAndRefreshSessionViewport(makeClearBodyColorCommand(bodyId));
    },
    clearFaceColor: async (faceId: string) => {
      await sendAndRefreshSessionViewport(makeClearFaceColorCommand(faceId));
    },
    clearAppearanceOverrides: async () => {
      await sendAndRefreshSessionViewport(makeClearAppearanceOverridesCommand());
    },
    createFillet: async (edgeIds: readonly string[], radius: number) => {
      await sendAndRefreshSessionViewport(
        makeCreateFilletCommand(edgeIds, radius),
      );
    },
    updateFilletRadius: async (featureId: string, radius: number) => {
      await sendAndRefreshSessionViewport(
        makeUpdateFilletRadiusCommand(featureId, radius),
      );
    },
    updateFilletEdges: async (
      featureId: string,
      edgeIds: readonly string[],
    ) => {
      await sendAndRefreshSessionViewport(
        makeUpdateFilletEdgesCommand(featureId, edgeIds),
      );
    },
    createChamfer: async (edgeIds: readonly string[], distance: number) => {
      await sendAndRefreshSessionViewport(
        makeCreateChamferCommand(edgeIds, distance),
      );
    },
    updateChamferDistance: async (featureId: string, distance: number) => {
      await sendAndRefreshSessionViewport(
        makeUpdateChamferDistanceCommand(featureId, distance),
      );
    },
    updateChamferEdges: async (
      featureId: string,
      edgeIds: readonly string[],
    ) => {
      await sendAndRefreshSessionViewport(
        makeUpdateChamferEdgesCommand(featureId, edgeIds),
      );
    },
    confirmFillet: async (featureId: string) => {
      await sendAndRefreshSessionViewport(makeConfirmFilletCommand(featureId));
    },
    confirmChamfer: async (featureId: string) => {
      await sendAndRefreshSessionViewport(makeConfirmChamferCommand(featureId));
    },
    createShell: async (faceId: string, thickness: number) => {
      await sendAndRefreshSessionViewport(
        makeCreateShellCommand(faceId, thickness),
      );
    },
    updateShellThickness: async (featureId: string, thickness: number) => {
      await sendAndRefreshSessionViewport(
        makeUpdateShellThicknessCommand(featureId, thickness),
      );
    },
    confirmShell: async (featureId: string) => {
      await sendAndRefreshSessionViewport(makeConfirmShellCommand(featureId));
    },
    createOffsetPlane: async (sourcePlaneId: string, offset: number) => {
      await sendAndRefreshSessionViewport(
        makeCreateOffsetPlaneCommand(sourcePlaneId, offset),
      );
    },
    createMidplane: async (sourcePlaneIds: [string, string]) => {
      await sendAndRefreshSessionViewport(
        makeCreateMidplaneCommand(sourcePlaneIds),
      );
    },
    createTangentPlane: async (sourceFaceId: string) => {
      await sendAndRefreshSessionViewport(
        makeCreateTangentPlaneCommand(sourceFaceId),
      );
    },
    createAnglePlane: async (
      sourcePlaneId: string,
      sourceAxisId: string,
      angleDegrees: number,
    ) => {
      await sendAndRefreshSessionViewport(
        makeCreateAnglePlaneCommand(sourcePlaneId, sourceAxisId, angleDegrees),
      );
    },
    createConstructionAxis: async (sourceId: string) => {
      await sendCoreCommand(makeCreateConstructionAxisCommand(sourceId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createConstructionPoint: async (sourceId: string) => {
      await sendCoreCommand(makeCreateConstructionPointCommand(sourceId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createHole: async (
      faceId: string,
      center: { x: number; y: number; z: number },
      parameters: Partial<HoleFeatureParameters> = {},
    ) => {
      await sendCoreCommand(makeCreateHoleCommand(faceId, center, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateHoleParameters: async (
      featureId: string,
      parameters: Partial<HoleFeatureParameters>,
    ) => {
      await sendCoreCommand(makeUpdateHoleParametersCommand(featureId, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    confirmHole: async (featureId: string) => {
      await sendCoreCommand(makeConfirmHoleCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createHelix: async (
      axisSourceId: string,
      parameters: Partial<HelixFeatureParameters> = {},
    ) => {
      await sendCoreCommand(makeCreateHelixCommand(axisSourceId, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateHelixParameters: async (
      featureId: string,
      parameters: Partial<HelixFeatureParameters>,
    ) => {
      await sendCoreCommand(makeUpdateHelixParametersCommand(featureId, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createThread: async (parameters: Partial<ThreadFeatureParameters>) => {
      await sendCoreCommand(makeCreateThreadCommand(parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateThreadParameters: async (
      featureId: string,
      parameters: Partial<ThreadFeatureParameters>,
    ) => {
      await sendCoreCommand(makeUpdateThreadParametersCommand(featureId, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    confirmThread: async (featureId: string) => {
      await sendCoreCommand(makeConfirmThreadCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createFastener: async (
      parameters: Partial<FastenerFeatureParameters> = {},
    ) => {
      await sendCoreCommand(makeCreateFastenerCommand(parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateFastenerParameters: async (
      featureId: string,
      parameters: Partial<FastenerFeatureParameters>,
    ) => {
      await sendCoreCommand(
        makeUpdateFastenerParametersCommand(featureId, parameters),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createMove: async (
      targetBodyId: string,
      parameters: Partial<MoveFeatureParameters> = {},
    ) => {
      await sendCoreCommand(makeCreateMoveCommand(targetBodyId, parameters));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    createBodyCopy: async (
      sourceBodyId: string,
      copyMode: "linked" | "standalone" = "linked",
    ) => {
      await sendCoreCommand(makeCreateBodyCopyCommand(sourceBodyId, copyMode));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    unlinkBodyCopy: async (featureId: string) => {
      await sendCoreCommand(makeUnlinkBodyCopyCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateMoveParameters: async (
      featureId: string,
      parameters: Partial<MoveFeatureParameters>,
    ) => {
      await sendCoreCommand(
        makeUpdateMoveParametersCommand(featureId, parameters),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    confirmMove: async (featureId: string) => {
      await sendCoreCommand(makeConfirmMoveCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateOffsetPlane: async (featureId: string, offset: number) => {
      await sendCoreCommand(makeUpdateOffsetPlaneCommand(featureId, offset));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateAnglePlane: async (featureId: string, angleDegrees: number) => {
      await sendCoreCommand(
        makeUpdateAnglePlaneCommand(featureId, angleDegrees),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    startSketchOnPlane: async (referenceId: string) => {
      await sendCoreCommand(makeStartSketchOnPlaneCommand(referenceId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    startSketchOnFace: async (
      faceId: string,
      planeFrame: SketchPlaneFramePayload,
    ) => {
      await sendCoreCommand(makeStartSketchOnFaceCommand(faceId, planeFrame));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchTool: async (tool: SketchTool) => {
      await sendCoreCommand(makeSetSketchToolCommand(tool));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchLine: async (
      lineId: string,
      startX: number,
      startY: number,
      endX: number,
      endY: number,
    ) => {
      await sendCoreCommand(
        makeUpdateSketchLineCommand(lineId, startX, startY, endX, endY),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchPoint: async (vertexId: string, x: number, y: number) => {
      await sendCoreCommand(makeUpdateSketchVertexCommand(vertexId, x, y));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    moveSketchEntities: async (params: {
      entityIds: string[];
      dx: number;
      dy: number;
      centerX: number;
      centerY: number;
      angleDeg: number;
    }) => {
      await sendCoreCommand(makeMoveSketchEntitiesCommand(params));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchLineConstraint: async (
      lineId: string,
      constraint: "none" | "horizontal" | "vertical",
    ) => {
      await sendCoreCommand(
        makeSetSketchLineConstraintCommand(lineId, constraint),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    clearSketchLineConstraints: async (lineId: string) => {
      await sendCoreCommand(
        makeClearSketchLineConstraintsCommand(lineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchEqualLengthConstraint: async (
      lineId: string,
      otherLineId: string | null,
    ) => {
      await sendCoreCommand(
        makeSetSketchEqualLengthConstraintCommand(lineId, otherLineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchPerpendicularConstraint: async (
      lineId: string,
      otherLineId: string | null,
    ) => {
      await sendCoreCommand(
        makeSetSketchPerpendicularConstraintCommand(lineId, otherLineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchTangentConstraint: async (lineId: string, circleId: string) => {
      await sendCoreCommand(
        makeSetSketchTangentConstraintCommand(lineId, circleId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    // Mirror tool — five-call lifecycle. Start opens the panel,
    // update_* drive the live preview, commit/cancel finish.
    startMirrorPreview: async () => {
      await sendCoreCommand(makeStartMirrorPreviewCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateMirrorPreviewAxis: async (axisLineId: string | null) => {
      await sendCoreCommand(makeUpdateMirrorPreviewAxisCommand(axisLineId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateMirrorPreviewObjects: async (objectIds: string[]) => {
      await sendCoreCommand(makeUpdateMirrorPreviewObjectsCommand(objectIds));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    commitMirrorPreview: async (persistent: boolean = false) => {
      await sendCoreCommand(makeCommitMirrorPreviewCommand(persistent));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    cancelMirrorPreview: async () => {
      await sendCoreCommand(makeCancelMirrorPreviewCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchParallelConstraint: async (
      lineId: string,
      otherLineId: string | null,
    ) => {
      await sendCoreCommand(
        makeSetSketchParallelConstraintCommand(lineId, otherLineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchCoincidentConstraint: async (
      vertexId: string,
      otherVertexId: string,
    ) => {
      await sendCoreCommand(
        makeSetSketchCoincidentConstraintCommand(vertexId, otherVertexId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchPointFixed: async (vertexId: string, isFixed: boolean) => {
      await sendCoreCommand(makeSetSketchVertexFixedCommand(vertexId, isFixed));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteSketchCoincidentConstraint: async (constraintId: string) => {
      await sendCoreCommand(
        makeDeleteSketchCoincidentConstraintCommand(constraintId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchCircle: async (
      circleId: string,
      centerX: number,
      centerY: number,
      radius: number,
    ) => {
      await sendCoreCommand(
        makeUpdateSketchCircleCommand(circleId, centerX, centerY, radius),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchDimension: async (dimensionId: string, value: number | string) => {
      await sendCoreCommand(
        makeUpdateSketchDimensionCommand(dimensionId, value),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchDimensionLabelPosition: async (
      dimensionId: string,
      labelX: number,
      labelY: number,
    ) => {
      await sendCoreCommand(
        makeUpdateSketchDimensionLabelPositionCommand(
          dimensionId,
          labelX,
          labelY,
        ),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchLine: async (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction = false,
    ) => {
      await sendCoreCommand(
        makeAddSketchLineCommand(startX, startY, endX, endY, isConstruction),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchLineConstruction: async (
      lineId: string,
      isConstruction: boolean,
    ) => {
      await sendCoreCommand(
        makeSetSketchLineConstructionCommand(lineId, isConstruction),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchMidpointAnchor: async (vertexId: string, hostLineId: string) => {
      await sendCoreCommand(
        makeSetSketchMidpointAnchorCommand(vertexId, hostLineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchPointLineAnchor: async (
      vertexId: string,
      hostLineId: string,
      t: number,
    ) => {
      await sendCoreCommand(
        makeSetSketchVertexLineAnchorCommand(vertexId, hostLineId, t),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchAngleDimension: async (
      firstLineId: string,
      secondLineId: string,
      value?: number,
    ) => {
      await sendCoreCommand(
        makeAddSketchAngleDimensionCommand(firstLineId, secondLineId, value),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchVertexDistanceDimension: async (
      vertexAId: string,
      vertexBId: string,
      axis?: "x" | "y",
    ) => {
      await sendCoreCommand(
        makeAddSketchVertexDistanceDimensionCommand(vertexAId, vertexBId, axis),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchDistanceDimension: async (
      firstEntityId: string,
      secondEntityId: string,
    ) => {
      await sendCoreCommand(
        makeAddSketchDistanceDimensionCommand(firstEntityId, secondEntityId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchLineLengthDimension: async (lineId: string) => {
      await sendCoreCommand(
        makeAddSketchLineLengthDimensionCommand(lineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchLineAngleDimension: async (lineId: string) => {
      await sendCoreCommand(
        makeAddSketchLineAngleDimensionCommand(lineId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchCircleRadiusDimension: async (
      circleId: string,
      displayAs?: string,
    ) => {
      await sendCoreCommand(
        makeAddSketchCircleRadiusDimensionCommand(circleId, displayAs),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchArcRadiusDimension: async (arcId: string) => {
      await sendCoreCommand(
        makeAddSketchArcRadiusDimensionCommand(arcId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchPolygonRadiusDimension: async (polygonId: string) => {
      await sendCoreCommand(
        makeAddSketchPolygonRadiusDimensionCommand(polygonId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchRectangle: async (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction = false,
    ) => {
      await sendCoreCommand(
        makeAddSketchRectangleCommand(
          startX,
          startY,
          endX,
          endY,
          isConstruction,
        ),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchCircle: async (
      centerX: number,
      centerY: number,
      radius: number,
      isConstruction = false,
    ) => {
      await sendCoreCommand(
        makeAddSketchCircleCommand(centerX, centerY, radius, isConstruction),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchPolygon: async (
      sides: number,
      mode: string,
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction = false,
    ) => {
      await sendCoreCommand(
        makeAddSketchPolygonCommand(sides, mode, startX, startY, endX, endY, isConstruction),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchArc: async (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      anchorX: number,
      anchorY: number,
      mode: "three_point" | "center_start_end",
      isConstruction = false,
    ) => {
      await sendCoreCommand(
        makeAddSketchArcCommand(
          startX,
          startY,
          endX,
          endY,
          anchorX,
          anchorY,
          mode,
          isConstruction,
        ),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchFillet: async (
      cornerPointId: string,
      lineAId: string,
      lineBId: string,
      radius: number,
    ) => {
      await sendCoreCommand(
        makeAddSketchFilletCommand(cornerPointId, lineAId, lineBId, radius),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchFilletRadius: async (filletId: string, radius: number) => {
      await sendCoreCommand(
        makeUpdateSketchFilletRadiusCommand(filletId, radius),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteSketchFillet: async (filletId: string) => {
      await sendCoreCommand(makeDeleteSketchFilletCommand(filletId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteSketchDimension: async (dimensionId: string) => {
      await sendCoreCommand(makeDeleteSketchDimensionCommand(dimensionId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    toggleSketchDimensionDriven: async (dimensionId: string) => {
      await sendCoreCommand(makeToggleSketchDimensionDrivenCommand(dimensionId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSketchDimensionDisplay: async (
      dimensionId: string,
      displayAs: string,
    ) => {
      await sendCoreCommand(
        makeUpdateSketchDimensionDisplayCommand(dimensionId, displayAs),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addParameter: async (name: string, expression: string, kind: "length" | "angle" = "length") => {
      await sendCoreCommand(makeAddParameterCommand(name, expression, kind));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateParameter: async (name: string, expression: string, kind: "length" | "angle" = "length") => {
      await sendCoreCommand(makeUpdateParameterCommand(name, expression, kind));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSelectionFilter: async (filter: SelectionFilterUpdate) => {
      await sendCoreCommand(makeUpdateSelectionFilterCommand(filter));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteParameter: async (name: string) => {
      await sendCoreCommand(makeDeleteParameterCommand(name));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    trimSketchEntity: async (entityId: string, clickX: number, clickY: number) => {
      await sendCoreCommand(makeTrimSketchEntityCommand(entityId, clickX, clickY));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteSketchSelection: async (
      entityIds: readonly string[],
      vertexIds: readonly string[],
      profileIds: readonly string[],
    ) => {
      await sendCoreCommand(
        makeDeleteSketchSelectionCommand(entityIds, vertexIds, profileIds),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchPoint: async (vertexId: string, additive = false) => {
      await sendCoreCommand(makeSelectSketchVertexCommand(vertexId, additive));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchEntity: async (entityId: string, additive = false) => {
      await sendCoreCommand(makeSelectSketchEntityCommand(entityId, additive));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchDimension: async (dimensionId: string) => {
      await sendCoreCommand(makeSelectSketchDimensionCommand(dimensionId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchProfile: async (profileId: string, additive = false) => {
      await sendCoreCommand(makeSelectSketchProfileCommand(profileId, additive));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    extrudeProfile: async (
      profileIds: string | readonly string[],
      depth: number,
      mode: ExtrudeMode | null = null,
      targetBodyId: string | null = null,
      parameters: Partial<ExtrudeAdvancedParameters> | null = null,
    ) => {
      await sendCoreCommand(
        makeExtrudeProfileCommand(
          profileIds,
          depth,
          mode,
          targetBodyId,
          parameters,
        ),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    extrudeOpenEntities: async (
      entityIds: readonly string[],
      depth: number,
      mode: ExtrudeMode | null = null,
      targetBodyId: string | null = null,
      parameters: Partial<ExtrudeAdvancedParameters> | null = null,
    ) => {
      await sendCoreCommand(
        makeExtrudeOpenEntitiesCommand(
          entityIds,
          depth,
          mode,
          targetBodyId,
          parameters,
        ),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    extrudeFace: async (
      faceId: string,
      depth: number,
      mode: ExtrudeMode | null = null,
      targetBodyId: string | null = null,
      parameters: Partial<ExtrudeAdvancedParameters> | null = null,
    ) => {
      await sendCoreCommand(
        makeExtrudeFaceCommand(faceId, depth, mode, targetBodyId, parameters),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateExtrudeMode: async (featureId: string, mode: ExtrudeMode) => {
      await sendCoreCommand(makeUpdateExtrudeModeCommand(featureId, mode));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateExtrudeTargetBody: async (
      featureId: string,
      targetBodyId: string | null,
    ) => {
      await sendCoreCommand(
        makeUpdateExtrudeTargetBodyCommand(featureId, targetBodyId),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateExtrudeParameters: async (
      featureId: string,
      parameters: ExtrudeFeatureParameters,
    ) => {
      await sendCoreCommand(
        makeUpdateExtrudeParametersCommand(featureId, parameters),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateExtrudeProfiles: async (
      featureId: string,
      profileIds: readonly string[],
    ) => {
      await sendCoreCommand(
        makeUpdateExtrudeProfilesCommand(featureId, profileIds),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    loftProfiles: async (profileIds: readonly string[], ruled = false) => {
      await sendCoreCommand(makeLoftProfilesCommand(profileIds, ruled));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateLoftProfiles: async (
      featureId: string,
      profileIds: readonly string[],
    ) => {
      await sendCoreCommand(makeUpdateLoftProfilesCommand(featureId, profileIds));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateLoftRuled: async (featureId: string, ruled: boolean) => {
      await sendCoreCommand(makeUpdateLoftRuledCommand(featureId, ruled));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    revolveProfile: async (
      profileId: string,
      axisEntityId: string,
      angleDegrees = 360,
    ) => {
      await sendCoreCommand(
        makeRevolveProfileCommand(profileId, axisEntityId, angleDegrees),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateRevolveProfile: async (featureId: string, profileId: string) => {
      await sendCoreCommand(makeUpdateRevolveProfileCommand(featureId, profileId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateRevolveAxis: async (featureId: string, axisEntityId: string) => {
      await sendCoreCommand(makeUpdateRevolveAxisCommand(featureId, axisEntityId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateRevolveAngle: async (featureId: string, angleDegrees: number) => {
      await sendCoreCommand(
        makeUpdateRevolveAngleCommand(featureId, angleDegrees),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    sweepProfile: async (profileId: string, pathEntityId: string) => {
      await sendCoreCommand(makeSweepProfileCommand(profileId, pathEntityId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSweepProfile: async (featureId: string, profileId: string) => {
      await sendCoreCommand(makeUpdateSweepProfileCommand(featureId, profileId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateSweepPath: async (featureId: string, pathEntityId: string) => {
      await sendCoreCommand(makeUpdateSweepPathCommand(featureId, pathEntityId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    finishSketch: async () => {
      await sendCoreCommand(makeFinishSketchCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    reenterSketch: async (featureId: string) => {
      await sendCoreCommand(makeReenterSketchCommand(featureId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    batchSelectSketchEntities: async (entityIds: string[], additive: boolean) => {
      if (!additive) {
        await sendCoreCommand(makeClearSelectionCommand());
      }
      // Fire all select commands in parallel — no need to wait for
      // each one's viewport state; one final refresh at the end is enough.
      await Promise.all(
        entityIds.map((id) =>
          sendCoreCommand(makeSelectSketchEntityCommand(id, true)),
        ),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    clearSelection: async () => {
      await sendCoreCommand(makeClearSelectionCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    camSetupCreate: async () => {
      await sendCoreCommand(makeCamSetupCreateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    camSetupUpdate: async (payload: import("@/lib/ipcProtocol").CamSetupUpdatePayload) => {
      await sendCoreCommand(makeCamSetupUpdateCommand(payload));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    camFaceMillingCreate: async (bodyId: string, faceIndex: number) => {
      await sendCoreCommand(makeCamOperationCreateCommand("face_milling", bodyId, faceIndex));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    camOperationUpdate: async (payload: import("@/lib/ipcProtocol").CamOperationUpdatePayload) => {
      await sendCoreCommand(makeCamOperationUpdateCommand(payload));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    camOperationDelete: async (operationId: string) => {
      await sendCoreCommand(makeCamOperationDeleteCommand(operationId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
  };
}
