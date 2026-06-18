import type {
  SelectionFilter,
  SketchDimensionEntry,
  SketchDimensionScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
} from "@/types";
import { type DimensionRelationPreview } from "./draftDimensions";
import {
  buildCircleDimensionRelationPreview,
  buildLineDimensionRelationPreview,
} from "./dimensionRelationPreviewSearch";
export { sketchLinesShareEndpoint } from "./dimensionRelationPreviewGeometry";

interface PreviewBuildContext {
  firstEntityId: string | null;
  activeSketchTool: string | null;
  sketchParameters: SketchFeatureParameters | null;
  filter: SelectionFilter;
  cursor: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  worldUnitsPerPixel: number;
  dimensionToolMode?: string;
}

interface AnglePreviewState {
  shouldApply: boolean;
  angle: number;
}

export interface DimensionRelationPreviewResult {
  relation: DimensionRelationPreview;
  dimension: SketchDimensionScene;
  anglePreview: AnglePreviewState | null;
}

function dimensionRelationMatchesCoreDimension(
  dimension: SketchDimensionEntry,
  relation: DimensionRelationPreview,
) {
  const isSamePair =
    (dimension.entity_id === relation.firstEntityId &&
      dimension.secondary_entity_id === relation.targetEntityId) ||
    (dimension.entity_id === relation.targetEntityId &&
      dimension.secondary_entity_id === relation.firstEntityId);
  if (!isSamePair) {
    return false;
  }
  if (relation.kind === "parallel_line_distance") {
    return dimension.kind === "line_line_distance";
  }
  if (relation.kind === "line_angle") {
    return dimension.kind === "angle";
  }
  if (relation.kind === "circle_center_distance") {
    return dimension.kind === "circle_center_distance";
  }
  return dimension.kind === "circle_line_distance";
}

export function pendingRelationDimension(
  relation: DimensionRelationPreview,
  dimensions: SketchDimensionScene[],
  sketchParameters: SketchFeatureParameters | null,
) {
  const coreDimension = sketchParameters?.dimensions.find((candidate) =>
    dimensionRelationMatchesCoreDimension(candidate, relation),
  );
  if (!coreDimension) {
    return null;
  }
  return (
    dimensions.find(
      (dimension) => dimension.dimensionId === coreDimension.dimension_id,
    ) ?? null
  );
}

export function buildDimensionRelationPreview({
  firstEntityId,
  activeSketchTool,
  sketchParameters,
  filter,
  cursor,
  planeId,
  planeFrame,
  worldUnitsPerPixel,
  dimensionToolMode,
}: PreviewBuildContext): DimensionRelationPreviewResult | null {
  if (!firstEntityId || !sketchParameters || activeSketchTool !== "dimension") {
    return null;
  }
  if (!filter.select_curves) {
    return null;
  }

  const allowConstruction = Boolean(filter.select_construction);

  return (
    buildLineDimensionRelationPreview({
      firstEntityId,
      sketchParameters,
      filter,
      cursor,
      planeId,
      planeFrame,
      allowConstruction,
      worldUnitsPerPixel,
      dimensionToolMode,
    }) ??
    buildCircleDimensionRelationPreview({
      firstEntityId,
      sketchParameters,
      filter,
      cursor,
      planeId,
      planeFrame,
      allowConstruction,
      worldUnitsPerPixel,
    })
  );
}
