import type { Dispatch, SetStateAction } from "react";

import { matchesHotkey } from "@/config";
import type { AppHotkeys } from "@/config/types";
import type { DocumentState, SketchTool } from "@/types";
import { isDraftDimensionTool, type DimensionLabelDragState } from "./draftDimensions";
import type { SelectedConstraintState } from "./contextMenuState";

interface MutableRef<T> {
  current: T;
}

interface SketchDeleteSelection {
  entityIds: string[];
  vertexIds: string[];
  profileIds: string[];
}

interface BindSketchHotkeysParams {
  activeSketchPlaneId: string | null;
  sketchToolbarHotkeys: AppHotkeys["sketchToolbar"];
  document: DocumentState | null;
  activeSketchToolRef: MutableRef<SketchTool>;
  dimensionLabelDragRef: MutableRef<DimensionLabelDragState | null>;
  dimensionPlacementOriginalPositionRef: MutableRef<
    [number, number, number] | null
  >;
  pendingDimensionIdRef: MutableRef<string | null>;
  pendingDimSourceEntityIdRef: MutableRef<string | null>;
  pendingDimensionPlacementRef: MutableRef<boolean>;
  controlsRef: MutableRef<{ enabled: boolean } | null>;
  selectedConstraintRef: MutableRef<SelectedConstraintState | null>;
  sketchToolConstructionRef: MutableRef<boolean>;
  deleteSketchDimensionRef: MutableRef<(dimensionId: string) => Promise<void>>;
  clearSketchConstraintRef: MutableRef<
    (
      kind: SelectedConstraintState["kind"],
      entityId: string,
      relatedEntityId: string | null,
    ) => Promise<void>
  >;
  deleteSketchSelectionRef: MutableRef<
    (selection?: SketchDeleteSelection) => Promise<void>
  >;
  setSketchToolRef: MutableRef<(tool: SketchTool) => Promise<void>>;
  clearPreviewDimension: () => void;
  finishDimensionPlacement: () => void;
  setCanvasCursor: (cursor: string) => void;
  setSelectedConstraint: (constraint: SelectedConstraintState | null) => void;
  cancelActiveSketchDraft: () => void;
  setSketchToolConstruction: Dispatch<SetStateAction<boolean>>;
}

export function bindSketchHotkeys({
  activeSketchPlaneId,
  ...handlerParams
}: BindSketchHotkeysParams) {
  if (!activeSketchPlaneId) {
    return undefined;
  }

  const handleKeyDown = (event: KeyboardEvent) => {
    handleSketchKeyDown(event, handlerParams);
  };

  window.addEventListener("keydown", handleKeyDown);
  return () => {
    window.removeEventListener("keydown", handleKeyDown);
  };
}

function handleSketchKeyDown(
  event: KeyboardEvent,
  params: Omit<BindSketchHotkeysParams, "activeSketchPlaneId">,
) {
  if (handleDimensionPlacementKey(event, params)) {
    return;
  }

  if (isTypingTarget(event.target)) {
    return;
  }

  if (handleSketchDeleteKey(event, params)) {
    return;
  }

  handleSketchToolHotkey(event, params);
}

function handleDimensionPlacementKey(
  event: KeyboardEvent,
  {
    activeSketchToolRef,
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    pendingDimensionIdRef,
    pendingDimSourceEntityIdRef,
    pendingDimensionPlacementRef,
    controlsRef,
    deleteSketchDimensionRef,
    clearPreviewDimension,
    finishDimensionPlacement,
    setCanvasCursor,
  }: Omit<BindSketchHotkeysParams, "activeSketchPlaneId">,
) {
  if (event.code === "Enter" && dimensionLabelDragRef.current?.isPlacement) {
    event.preventDefault();
    finishDimensionPlacement();
    return true;
  }

  if (event.code === "Escape" && dimensionLabelDragRef.current?.isPlacement) {
    event.preventDefault();
    const dimId = dimensionLabelDragRef.current.dimensionId;
    resetDimensionPlacement({
      dimensionLabelDragRef,
      dimensionPlacementOriginalPositionRef,
      controlsRef,
      setCanvasCursor,
    });
    void deleteSketchDimensionRef.current(dimId);
    return true;
  }

  if (event.code !== "Escape" || activeSketchToolRef.current !== "dimension") {
    return false;
  }

  return handlePendingDimensionEscape(event, {
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    pendingDimensionIdRef,
    pendingDimSourceEntityIdRef,
    pendingDimensionPlacementRef,
    controlsRef,
    deleteSketchDimensionRef,
    clearPreviewDimension,
    setCanvasCursor,
  });
}

function handlePendingDimensionEscape(
  event: KeyboardEvent,
  {
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    pendingDimensionIdRef,
    pendingDimSourceEntityIdRef,
    pendingDimensionPlacementRef,
    controlsRef,
    deleteSketchDimensionRef,
    clearPreviewDimension,
    setCanvasCursor,
  }: Pick<
    BindSketchHotkeysParams,
    | "dimensionLabelDragRef"
    | "dimensionPlacementOriginalPositionRef"
    | "pendingDimensionIdRef"
    | "pendingDimSourceEntityIdRef"
    | "pendingDimensionPlacementRef"
    | "controlsRef"
    | "deleteSketchDimensionRef"
    | "clearPreviewDimension"
    | "setCanvasCursor"
  >,
) {
  const drag = dimensionLabelDragRef.current;
  const targetId =
    pendingDimensionIdRef.current ??
    (drag?.isPlacement ? drag.dimensionId : null);
  if (!targetId) {
    return false;
  }

  event.preventDefault();
  clearPreviewDimension();
  pendingDimensionIdRef.current = null;
  pendingDimSourceEntityIdRef.current = null;
  pendingDimensionPlacementRef.current = false;
  resetDimensionPlacement({
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    controlsRef,
    setCanvasCursor,
  });
  void deleteSketchDimensionRef.current(targetId);
  return true;
}

function resetDimensionPlacement({
  dimensionLabelDragRef,
  dimensionPlacementOriginalPositionRef,
  controlsRef,
  setCanvasCursor,
}: Pick<
  BindSketchHotkeysParams,
  | "dimensionLabelDragRef"
  | "dimensionPlacementOriginalPositionRef"
  | "controlsRef"
  | "setCanvasCursor"
>) {
  dimensionLabelDragRef.current = null;
  dimensionPlacementOriginalPositionRef.current = null;
  if (controlsRef.current) {
    controlsRef.current.enabled = true;
  }
  setCanvasCursor("");
}

function handleSketchDeleteKey(
  event: KeyboardEvent,
  {
    document,
    selectedConstraintRef,
    clearSketchConstraintRef,
    deleteSketchSelectionRef,
    setSelectedConstraint,
    cancelActiveSketchDraft,
  }: Omit<BindSketchHotkeysParams, "activeSketchPlaneId">,
) {
  if (event.code === "Escape") {
    event.preventDefault();
    setSelectedConstraint(null);
    cancelActiveSketchDraft();
    return true;
  }

  if (event.code !== "Delete" && event.code !== "Backspace") {
    return false;
  }

  event.preventDefault();
  deleteSelectedSketchItems({
    document,
    selectedConstraintRef,
    setSelectedConstraint,
    clearSketchConstraint: clearSketchConstraintRef.current,
    deleteSketchSelection: deleteSketchSelectionRef.current,
  });
  return true;
}

function handleSketchToolHotkey(
  event: KeyboardEvent,
  {
    sketchToolbarHotkeys,
    activeSketchToolRef,
    sketchToolConstructionRef,
    setSketchToolRef,
    setSketchToolConstruction,
  }: Omit<BindSketchHotkeysParams, "activeSketchPlaneId">,
) {
  if (matchesHotkey(event, sketchToolbarHotkeys.line)) {
    event.preventDefault();
    void setSketchToolRef.current("line");
    return;
  }

  if (matchesHotkey(event, sketchToolbarHotkeys.rectangle)) {
    event.preventDefault();
    void setSketchToolRef.current("rectangle");
    return;
  }

  if (matchesHotkey(event, sketchToolbarHotkeys.circle)) {
    event.preventDefault();
    void setSketchToolRef.current("circle");
    return;
  }

  if (matchesHotkey(event, sketchToolbarHotkeys.trim)) {
    event.preventDefault();
    void setSketchToolRef.current("trim");
    return;
  }

  if (
    matchesHotkey(event, sketchToolbarHotkeys.toggleConstruction) &&
    isDraftDimensionTool(activeSketchToolRef.current)
  ) {
    event.preventDefault();
    setSketchToolConstruction((prev) => {
      const next = !prev;
      sketchToolConstructionRef.current = next;
      return next;
    });
    return;
  }

  if (matchesHotkey(event, sketchToolbarHotkeys.dimension)) {
    event.preventDefault();
    void setSketchToolRef.current("dimension");
  }
}

function isTypingTarget(target: EventTarget | null) {
  return (
    target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    (target instanceof HTMLElement && target.isContentEditable)
  );
}

function deleteSelectedSketchItems({
  document,
  selectedConstraintRef,
  setSelectedConstraint,
  clearSketchConstraint,
  deleteSketchSelection,
}: {
  document: DocumentState | null;
  selectedConstraintRef: MutableRef<SelectedConstraintState | null>;
  setSelectedConstraint: (constraint: SelectedConstraintState | null) => void;
  clearSketchConstraint: (
    kind: SelectedConstraintState["kind"],
    entityId: string,
    relatedEntityId: string | null,
  ) => Promise<void>;
  deleteSketchSelection: (selection?: SketchDeleteSelection) => Promise<void>;
}) {
  const selectedConstraint = selectedConstraintRef.current;
  if (selectedConstraint) {
    setSelectedConstraint(null);
    void clearSketchConstraint(
      selectedConstraint.kind,
      selectedConstraint.entityId,
      selectedConstraint.relatedEntityId ?? null,
    );
    return;
  }

  const entityIds = document?.selected_sketch_entity_ids ?? [];
  const entityId = document?.selected_sketch_entity_id;
  const vertexIds = document?.selected_sketch_vertex_ids ?? [];
  const profileIds = document?.selected_sketch_profile_ids ?? [];
  const allEntityIds = entityId
    ? entityIds.includes(entityId)
      ? entityIds
      : [...entityIds, entityId]
    : entityIds;

  if (allEntityIds.length > 0 || vertexIds.length > 0 || profileIds.length > 0) {
    void deleteSketchSelection({
      entityIds: allEntityIds,
      vertexIds,
      profileIds,
    });
    return;
  }

  void deleteSketchSelection();
}
