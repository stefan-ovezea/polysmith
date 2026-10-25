import * as THREE from "three";

import type {
  SelectionFilter,
  SketchDimensionScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchTool,
} from "@/types";
import { buildSketchDimensionObject } from "@/utils";
import type { DisplayUnits } from "@/utils/units";
import {
  buildDimensionRelationPreview,
} from "./dimensionRelationPreview";
import { unaryDimensionIdForEntity } from "./dimensionToolPicking";
import type { DimensionRelationPreview } from "./draftDimensions";

interface MutableRef<T> {
  current: T;
}

interface DimensionRelationPreviewActionsContext {
  displayUnits: DisplayUnits;
  sketchGroupRef: MutableRef<THREE.Group | null>;
  previewDimensionRef: MutableRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>;
  dimensionRelationPreviewRef: MutableRef<DimensionRelationPreview | null>;
  dimensionRelationPreviewLabelRef: MutableRef<
    [number, number, number] | null
  >;
  dimensionToolFirstLineRef: MutableRef<string | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  activeSketchToolRef: MutableRef<SketchTool>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  pendingAnglePreviewAppliesRef: MutableRef<boolean>;
  pendingAnglePreviewValueRef: MutableRef<number>;
  worldUnitsPerPixelRef: MutableRef<number>;
  clearPreviewDimension: () => void;
  hideRelationPreviewDimension: (dimensionId: string | null) => void;
  readDimensionPreviewFilter: () => SelectionFilter;
  dimensionToolModeRef: MutableRef<string>;
}

export function createDimensionRelationPreviewActions({
  displayUnits,
  sketchGroupRef,
  previewDimensionRef,
  dimensionRelationPreviewRef,
  dimensionRelationPreviewLabelRef,
  dimensionToolFirstLineRef,
  sketchLinesRef,
  activeSketchToolRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  pendingAnglePreviewAppliesRef,
  pendingAnglePreviewValueRef,
  worldUnitsPerPixelRef,
  clearPreviewDimension,
  hideRelationPreviewDimension,
  readDimensionPreviewFilter,
  dimensionToolModeRef,
}: DimensionRelationPreviewActionsContext) {
  function renderDimensionRelationPreview(
    relation: DimensionRelationPreview,
    dimension: SketchDimensionScene,
  ) {
    clearPreviewDimension();
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) {
      dimensionRelationPreviewRef.current = null;
      return;
    }
    const preview = buildSketchDimensionObject(dimension, displayUnits, {
      variant: "muted-preview",
      pickable: false,
    });
    hideRelationPreviewDimension(
      unaryDimensionIdForEntity(relation.firstEntityId),
    );
    previewDimensionRef.current = preview;
    dimensionRelationPreviewRef.current = relation;
    dimensionRelationPreviewLabelRef.current = dimension.labelPosition;
    sketchGroup.add(preview.line);
    sketchGroup.add(preview.label);
  }

  function updateDimensionRelationPreview(cursor: [number, number]) {
    clearPreviewDimension();
    dimensionRelationPreviewRef.current = null;
    const preview = buildDimensionRelationPreview({
      firstEntityId: dimensionToolFirstLineRef.current,
      activeSketchTool: activeSketchToolRef.current,
      sketchParameters: sketchLinesRef.current,
      filter: readDimensionPreviewFilter(),
      cursor,
      planeId: activeSketchPlaneIdRef.current ?? "ref-plane-xy",
      planeFrame: activeSketchPlaneFrameRef.current,
      worldUnitsPerPixel: worldUnitsPerPixelRef.current,
      dimensionToolMode: dimensionToolModeRef.current,
    });
    if (!preview) {
      pendingAnglePreviewAppliesRef.current = false;
      pendingAnglePreviewValueRef.current = 0;
      return null;
    }
    if (preview.anglePreview) {
      pendingAnglePreviewAppliesRef.current = preview.anglePreview.shouldApply;
      // Signed radians (first → second): add_sketch_angle_dimension
      // stores this directed value so the solver and a later
      // "Toggle Driving" agree with the quadrant the user picked.
      pendingAnglePreviewValueRef.current = preview.anglePreview.shouldApply
        ? preview.anglePreview.signedAngle
        : 0;
    } else {
      pendingAnglePreviewAppliesRef.current = false;
      pendingAnglePreviewValueRef.current = 0;
    }
    renderDimensionRelationPreview(preview.relation, preview.dimension);
    return preview.relation;
  }

  return {
    updateDimensionRelationPreview,
  };
}
