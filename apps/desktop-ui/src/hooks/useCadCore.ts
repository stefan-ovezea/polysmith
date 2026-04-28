import { useEffect } from "react";
import {
  onCadCoreError,
  onCadCoreEvent,
  onCadCoreExited,
  onCadCoreLog,
  sendCoreCommand,
  startCadCore,
  makeCreateDocumentCommand,
  makeAddBoxFeatureCommand,
  makeAddCylinderFeatureCommand,
  makeAddSketchCircleCommand,
  makeAddSketchLineCommand,
  makeAddSketchRectangleCommand,
  makeClearSelectionCommand,
  makeDeleteFeatureCommand,
  makeExportDocumentCommand,
  makeExportDocumentStlCommand,
  makeLoadDocumentCommand,
  makeProjectFaceIntoSketchCommand,
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
  makeSelectSketchPointCommand,
  makeRenameFeatureCommand,
  makeSelectFeatureCommand,
  makeSelectReferenceCommand,
  makeSelectFaceCommand,
  makeSetSketchCoincidentConstraintCommand,
  makeSetSketchEqualLengthConstraintCommand,
  makeSetSketchParallelConstraintCommand,
  makeSetSketchPerpendicularConstraintCommand,
  makeSetSketchPointFixedCommand,
  makeExtrudeProfileCommand,
  makeSetSketchLineConstraintCommand,
  makeSetSketchToolCommand,
  makeStartSketchOnPlaneCommand,
  makeStartSketchOnFaceCommand,
  makeUndoCommand,
  makeUpdateSketchCircleCommand,
  makeUpdateSketchDimensionCommand,
  makeUpdateSketchLineCommand,
  makeUpdateSketchPointCommand,
  makeUpdateBoxFeatureCommand,
  makeUpdateExtrudeDepthCommand,
  parseCoreMessage,
} from "@/lib";

import { useCadCoreStore } from "@/state";
import { SketchTool } from "@/types";

export function useCadCore() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const handleCoreMessage = useCadCoreStore((state) => state.handleCoreMessage);
  const setStatus = useCadCoreStore((state) => state.setStatus);

  useEffect(() => {
    let disposed = false;
    const unlistenFns: Array<() => void> = [];

    async function setupListeners() {
      const unlistenEvent = await onCadCoreEvent((payload) => {
        try {
          const message = parseCoreMessage(payload);
          handleCoreMessage(message);
        } catch (error) {
          addMessage(`parse error: ${String(error)}`);
          setStatus("error");
        }
      });

      const unlistenLog = await onCadCoreLog((line) => {
        addMessage(`log: ${line}`);
      });

      const unlistenError = await onCadCoreError((message) => {
        addMessage(`bridge error: ${message}`);
        setStatus("error");
      });

      const unlistenExited = await onCadCoreExited((message) => {
        addMessage(`exit: ${message}`);
        setStatus("stopped");
      });

      for (const unlisten of [
        unlistenEvent,
        unlistenLog,
        unlistenError,
        unlistenExited,
      ]) {
        if (disposed) {
          unlisten();
        } else {
          unlistenFns.push(unlisten);
        }
      }
    }

    void setupListeners();

    return () => {
      disposed = true;
      for (const unlisten of unlistenFns) {
        unlisten();
      }
    };
  }, [addMessage, handleCoreMessage, setStatus]);

  return {
    start: async () => {
      setStatus("starting");
      const result = await startCadCore();
      addMessage(`start: ${result}`);
    },
    ping: async () => {
      await sendCoreCommand(makePingCommand());
    },
    createDocument: async () => {
      await sendCoreCommand(makeCreateDocumentCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
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
    projectFaceIntoSketch: async (faceId: string) => {
      await sendCoreCommand(makeProjectFaceIntoSketchCommand(faceId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addBoxFeature: async (width: number, height: number, depth: number) => {
      await sendCoreCommand(makeAddBoxFeatureCommand(width, height, depth));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addCylinderFeature: async (radius: number, height: number) => {
      await sendCoreCommand(makeAddCylinderFeatureCommand(radius, height));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateBoxFeature: async (
      featureId: string,
      width: number,
      height: number,
      depth: number,
    ) => {
      await sendCoreCommand(
        makeUpdateBoxFeatureCommand(featureId, width, height, depth),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateExtrudeDepth: async (featureId: string, depth: number) => {
      await sendCoreCommand(makeUpdateExtrudeDepthCommand(featureId, depth));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    renameFeature: async (featureId: string, name: string) => {
      await sendCoreCommand(makeRenameFeatureCommand(featureId, name));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteFeature: async (featureId: string) => {
      await sendCoreCommand(makeDeleteFeatureCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    undo: async () => {
      await sendCoreCommand(makeUndoCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    redo: async () => {
      await sendCoreCommand(makeRedoCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectFeature: async (featureId: string) => {
      await sendCoreCommand(makeSelectFeatureCommand(featureId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectReference: async (referenceId: string) => {
      await sendCoreCommand(makeSelectReferenceCommand(referenceId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectFace: async (faceId: string) => {
      await sendCoreCommand(makeSelectFaceCommand(faceId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    startSketchOnPlane: async (referenceId: string) => {
      await sendCoreCommand(makeStartSketchOnPlaneCommand(referenceId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    startSketchOnFace: async (
      faceId: string,
      planeFrame: {
        origin: { x: number; y: number; z: number };
        x_axis: { x: number; y: number; z: number };
        y_axis: { x: number; y: number; z: number };
        normal: { x: number; y: number; z: number };
      },
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
    updateSketchPoint: async (pointId: string, x: number, y: number) => {
      await sendCoreCommand(makeUpdateSketchPointCommand(pointId, x, y));
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
      pointId: string,
      otherPointId: string,
    ) => {
      await sendCoreCommand(
        makeSetSketchCoincidentConstraintCommand(pointId, otherPointId),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    setSketchPointFixed: async (pointId: string, isFixed: boolean) => {
      await sendCoreCommand(makeSetSketchPointFixedCommand(pointId, isFixed));
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
    updateSketchDimension: async (dimensionId: string, value: number) => {
      await sendCoreCommand(
        makeUpdateSketchDimensionCommand(dimensionId, value),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchLine: async (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
    ) => {
      await sendCoreCommand(
        makeAddSketchLineCommand(startX, startY, endX, endY),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchRectangle: async (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
    ) => {
      await sendCoreCommand(
        makeAddSketchRectangleCommand(startX, startY, endX, endY),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addSketchCircle: async (
      centerX: number,
      centerY: number,
      radius: number,
    ) => {
      await sendCoreCommand(
        makeAddSketchCircleCommand(centerX, centerY, radius),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchPoint: async (pointId: string) => {
      await sendCoreCommand(makeSelectSketchPointCommand(pointId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchEntity: async (entityId: string) => {
      await sendCoreCommand(makeSelectSketchEntityCommand(entityId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchDimension: async (dimensionId: string) => {
      await sendCoreCommand(makeSelectSketchDimensionCommand(dimensionId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectSketchProfile: async (profileId: string) => {
      await sendCoreCommand(makeSelectSketchProfileCommand(profileId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    extrudeProfile: async (profileId: string, depth: number) => {
      await sendCoreCommand(makeExtrudeProfileCommand(profileId, depth));
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
    clearSelection: async () => {
      await sendCoreCommand(makeClearSelectionCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
  };
}
