import * as THREE from "three";

import type {
  ArmedSketchConstraint,
  SketchFeatureParameters,
  SketchTool,
} from "@/types";
import {
  handleDimensionToolClick,
} from "./dimensionToolPicking";
import { handleSketchFilletClick } from "./sketchFilletPicking";
import { handleSketchTrimClick, type TrimPlaneFrame } from "./trimHoverPreview";
import type { SelectedConstraintState } from "./contextMenuState";
import {
  handleActiveSketchProjectHit,
  handleActiveSketchSelectHit,
  type ActiveSketchSelectHit,
} from "./sketchClickSelection";

export interface DimensionFirstPoint {
  id: string;
  x: number;
  y: number;
}

export interface FilletPoint {
  local: [number, number];
}

export interface ActiveSketchPointerUpContext {
  activeSketchTool: SketchTool;
  hit: ActiveSketchSelectHit;
  additiveSelection: boolean;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
  sketch: SketchFeatureParameters | null;
  armedSketchConstraint: ArmedSketchConstraint;
  mirrorFocusedSlot: "objects" | "axis" | null;
  inactiveSketchEntityPickEnabled: boolean;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjects: readonly THREE.Mesh[];
  resolveFilletPoint: () => FilletPoint | null;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  selectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
  trimSketchEntity:
    | ((entityId: string, localX: number, localY: number) => Promise<void>)
    | null
    | undefined;
  mirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  pickSketchPoint: (
    pointId: string,
    pointKind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  handleDimensionClick: (dimensionId: string) => void;
  setIsDimensionEditorOpen: (open: boolean) => void;
  setSelectedConstraint: (constraint: SelectedConstraintState) => void;
  paintSketchEntityMaterials: () => void;
  paintSketchPointMaterials: () => void;
  addMessage: (message: string) => void;
  addSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  pendingDimensionPlacement: boolean;
  pendingDimensionSourceId: string | null;
  pendingDimensionId: string | null;
  getDimensionFirstEntityId: () => string | null;
  getDimensionFirstPoint: () => DimensionFirstPoint | null;
  clearDimensionFirstPick: () => void;
  clearDimensionFirstEntity: () => void;
  clearPendingDimensionPlacement: () => void;
  stageDimensionFirstEntity: (entityId: string) => void;
  stageDimensionFirstPoint: (point: DimensionFirstPoint) => void;
  deleteSketchDimension: (dimensionId: string) => void;
  createDimensionAngleOrDistance: (
    firstEntityId: string,
    secondEntityId: string,
    forceMode?: "angle" | "distance",
  ) => void;
  createDimensionPointDistance: (
    firstPointId: string,
    secondPointId: string,
  ) => void;
  createDimensionLine: (lineId: string) => void;
  createDimensionCircle: (circleId: string, label: string) => void;
  selectDimensionCircle: (circleId: string) => void;
  createDimensionPolygon: (polygonId: string) => void;
  selectDimensionPolygon: (polygonId: string) => void;
  selectDimensionLine: (lineId: string) => void;
  dimensionToolMode: import("@/types").DimensionToolMode;
}

function dimensionToolHit(hit: ActiveSketchSelectHit) {
  return hit?.kind === "sketch_dimension" ||
    hit?.kind === "sketch_entity" ||
    hit?.kind === "sketch_point"
    ? hit
    : null;
}

export function handleActiveSketchPointerUpTool(
  context: ActiveSketchPointerUpContext,
) {
  if (context.activeSketchTool === "project") {
    handleActiveSketchProjectHit({
      hit: context.hit,
      selectSketchProfile: context.selectSketchProfile,
      selectVertex: context.selectVertex,
      selectEdge: context.selectEdge,
      selectFace: context.selectFace,
    });
    return true;
  }

  if (context.activeSketchTool === "trim") {
    handleSketchTrimClick({
      hit: context.hit?.kind === "sketch_entity" ? context.hit : null,
      planeId: context.planeId,
      planeFrame: context.planeFrame,
      trimSketchEntity: context.trimSketchEntity,
    });
    return true;
  }

  if (context.activeSketchTool === "select") {
    handleActiveSketchSelectHit({
      hit: context.hit,
      additiveSelection: context.additiveSelection,
      armedSketchConstraint: context.armedSketchConstraint,
      mirrorFocusedSlot: context.mirrorFocusedSlot,
      inactiveSketchEntityPickEnabled: context.inactiveSketchEntityPickEnabled,
      sketchEntityObjectById: context.sketchEntityObjectById,
      sketchPointObjects: context.sketchPointObjects,
      mirrorEntityPick: context.mirrorEntityPick,
      selectSketchEntity: context.selectSketchEntity,
      pickSketchPoint: context.pickSketchPoint,
      handleDimensionClick: context.handleDimensionClick,
      setSelectedConstraint: context.setSelectedConstraint,
      paintSketchEntityMaterials: context.paintSketchEntityMaterials,
      paintSketchPointMaterials: context.paintSketchPointMaterials,
      selectSketchProfile: context.selectSketchProfile,
      addMessage: context.addMessage,
    });
    return true;
  }

  if (context.activeSketchTool === "dimension") {
    handleDimensionToolClick({
      hit: dimensionToolHit(context.hit),
      sketch: context.sketch,
      pendingPlacement: context.pendingDimensionPlacement,
      pendingSourceId: context.pendingDimensionSourceId,
      pendingDimensionId: context.pendingDimensionId,
      getFirstEntityId: context.getDimensionFirstEntityId,
      getFirstPoint: context.getDimensionFirstPoint,
      clearFirstPick: context.clearDimensionFirstPick,
      clearFirstEntity: context.clearDimensionFirstEntity,
      clearPendingPlacement: context.clearPendingDimensionPlacement,
      stageFirstEntity: context.stageDimensionFirstEntity,
      stageFirstPoint: context.stageDimensionFirstPoint,
      deleteSketchDimension: context.deleteSketchDimension,
      handleDimensionClick: context.handleDimensionClick,
      createAngleOrDistance: context.createDimensionAngleOrDistance,
      createPointDistance: context.createDimensionPointDistance,
      createLine: context.createDimensionLine,
      createCircle: context.createDimensionCircle,
      selectCircle: context.selectDimensionCircle,
      createPolygon: context.createDimensionPolygon,
      selectPolygon: context.selectDimensionPolygon,
      selectLine: context.selectDimensionLine,
      dimensionToolMode: context.dimensionToolMode,
    });
    return true;
  }

  if (context.activeSketchTool === "fillet") {
    const filletPoint = context.resolveFilletPoint();
    if (!filletPoint) {
      return true;
    }
    handleSketchFilletClick({
      sketch: context.sketch,
      localPoint: filletPoint.local,
      addSketchFillet: context.addSketchFillet,
    });
    return true;
  }

  return false;
}
