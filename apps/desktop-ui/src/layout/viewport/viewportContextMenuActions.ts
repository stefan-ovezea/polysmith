import type {
  ConstraintType,
  DocumentState,
  SketchFeatureParameters,
  ViewportContextMenuState,
  ViewportScene,
} from "@/types";
import type { SelectedConstraintState } from "./contextMenuState";

interface MutableRef<T> {
  current: T;
}

export function createViewportContextMenuActions({
  contextMenu,
  document,
  sceneData,
  sketchLinesRef,
  setContextMenu,
  setSelectedConstraint,
  setIsDimensionEditorOpen,
  selectReferenceRef,
  startSketchRef,
  selectFaceRef,
  startSketchOnFaceRef,
  moveBodyRef,
  copyBodyRef,
  exportBodyMeshRef,
  unlinkBodyCopyRef,
  deleteSketchSelectionRef,
  deleteSketchDimensionRef,
  toggleSketchDimensionDrivenRef,
  setSketchLineConstructionRef,
  clearSketchConstraintRef,
  updateSketchDimensionDisplayRef,
}: {
  contextMenu: ViewportContextMenuState | null;
  document: DocumentState | null;
  sceneData: ViewportScene | null;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  setContextMenu: (contextMenu: ViewportContextMenuState | null) => void;
  setSelectedConstraint: (constraint: SelectedConstraintState | null) => void;
  setIsDimensionEditorOpen: (isOpen: boolean) => void;
  selectReferenceRef: MutableRef<(referenceId: string) => Promise<void>>;
  startSketchRef: MutableRef<(referenceId: string) => Promise<void>>;
  selectFaceRef: MutableRef<(faceId: string) => Promise<void>>;
  startSketchOnFaceRef: MutableRef<
    (faceId: string, planeFrame: ViewportScene["solidFaces"][number]["planeFrame"]) => Promise<void>
  >;
  moveBodyRef: MutableRef<((bodyId: string) => Promise<void> | void) | undefined>;
  copyBodyRef: MutableRef<
    | ((
        bodyId: string,
        copyMode: "linked" | "standalone",
      ) => Promise<void> | void)
    | undefined
  >;
  exportBodyMeshRef: MutableRef<
    ((bodyId: string) => Promise<void> | void) | undefined
  >;
  unlinkBodyCopyRef: MutableRef<
    ((featureId: string) => Promise<void> | void) | undefined
  >;
  deleteSketchSelectionRef: MutableRef<
    (selection: NonNullable<ViewportContextMenuState["sketchDeleteSelection"]>) => Promise<void>
  >;
  deleteSketchDimensionRef: MutableRef<(dimensionId: string) => Promise<void>>;
  toggleSketchDimensionDrivenRef: MutableRef<(dimensionId: string) => Promise<void>>;
  setSketchLineConstructionRef: MutableRef<
    (lineId: string, isConstruction: boolean) => Promise<void>
  >;
  clearSketchConstraintRef: MutableRef<
    (
      kind: ConstraintType,
      entityId: string,
      relatedEntityId?: string | null,
    ) => Promise<void>
  >;
  updateSketchDimensionDisplayRef: MutableRef<
    (dimensionId: string, displayAs: string) => Promise<void>
  >;
}) {
  async function createSketch() {
    if (contextMenu?.referenceId) {
      setContextMenu(null);
      await selectReferenceRef.current(contextMenu.referenceId);
      await startSketchRef.current(contextMenu.referenceId);
      return;
    }

    if (!contextMenu?.faceId) {
      return;
    }

    setContextMenu(null);
    await selectFaceRef.current(contextMenu.faceId);

    const solidFace = sceneData?.solidFaces.find(
      (face) => face.faceId === contextMenu.faceId,
    );
    if (!solidFace) {
      return;
    }

    await startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
  }

  async function moveBody() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await moveBodyRef.current?.(bodyId);
  }

  async function copyBody(copyMode: "linked" | "standalone") {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await copyBodyRef.current?.(bodyId, copyMode);
  }

  async function exportBodyMesh() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await exportBodyMeshRef.current?.(bodyId);
  }

  async function unlinkBodyCopy() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await unlinkBodyCopyRef.current?.(bodyId);
  }

  async function deleteSketchSelection() {
    const selection = contextMenu?.sketchDeleteSelection;
    if (!selection) {
      return;
    }
    setContextMenu(null);
    await deleteSketchSelectionRef.current(selection);
  }

  async function deleteDimension() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) {
      return;
    }
    setContextMenu(null);
    setIsDimensionEditorOpen(false);
    await deleteSketchDimensionRef.current(dimensionId);
  }

  async function toggleDriven() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) {
      return;
    }
    setContextMenu(null);
    await toggleSketchDimensionDrivenRef.current(dimensionId);
  }

  async function toggleConstruction() {
    const lineId = contextMenu?.lineId;
    if (!lineId) {
      return;
    }

    // Look up the line's current construction state from the document
    // (always current), not from sketchLinesRef (only updated on line
    // commits and may be stale / null).
    let isConstruction = false;
    if (document) {
      const activeId = document.active_sketch_feature_id;
      const feature = activeId
        ? document.feature_history.find((f) => f.feature_id === activeId)
        : null;
      const line = feature?.sketch_parameters?.lines.find(
        (l) => l.line_id === lineId,
      );
      if (line) {
        isConstruction = line.is_construction;
      }
    }

    setContextMenu(null);
    try {
      await setSketchLineConstructionRef.current(lineId, !isConstruction);
    } catch {
      // Core may reject the command if the line is part of an active
      // profile used by a downstream extrude / loft / revolve / sweep.
      // Swallow the error — the menu is already closed.
    }
  }

  async function deleteConstraint() {
    const kind = contextMenu?.constraintKind;
    const entityId = contextMenu?.constraintEntityId;
    const constraintId = contextMenu?.constraintId;
    if (!kind || !entityId) {
      return;
    }
    setContextMenu(null);
    setSelectedConstraint(null);
    const deleteId =
      kind === "mirror" || kind === "coincident"
        ? constraintId ?? entityId
        : entityId;
    await clearSketchConstraintRef.current(
      kind as ConstraintType,
      deleteId,
      contextMenu?.constraintRelatedEntityId ?? null,
    );
  }

  async function toggleDimensionDisplay() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) {
      return;
    }
    const sketch = sketchLinesRef.current;
    if (!sketch) {
      return;
    }
    const dimension = sketch.dimensions.find(
      (entry) => entry.dimension_id === dimensionId,
    );
    if (!dimension || dimension.kind !== "circle_radius") {
      return;
    }

    const nextDisplayAs = dimension.display_as === "radius" ? "" : "radius";
    setContextMenu(null);
    await updateSketchDimensionDisplayRef.current(dimensionId, nextDisplayAs);
  }

  function getCircleDimensionToggleLabel(dimensionId: string) {
    const sketch = sketchLinesRef.current;
    if (!sketch) {
      return null;
    }
    const dimension = sketch.dimensions.find(
      (entry) => entry.dimension_id === dimensionId,
    );
    if (!dimension || dimension.kind !== "circle_radius") {
      return null;
    }
    return dimension.display_as === "radius" ? "Show Diameter" : "Show Radius";
  }

  function isLinkedBodyCopy(bodyId: string | null | undefined) {
    if (!bodyId) {
      return false;
    }
    const feature = document?.feature_history.find(
      (entry) => entry.feature_id === bodyId,
    );
    return (
      feature?.kind === "body_copy" &&
      feature.body_copy_parameters?.copy_mode === "linked"
    );
  }

  return {
    createSketch,
    moveBody,
    copyBody,
    exportBodyMesh,
    unlinkBodyCopy,
    deleteSketchSelection,
    deleteDimension,
    deleteConstraint,
    toggleDriven,
    toggleConstruction,
    toggleDimensionDisplay,
    getCircleDimensionToggleLabel,
    isLinkedBodyCopy,
  };
}
