import type { SketchDimensionScene, SketchFeatureParameters } from "@/types";
import { mmToDisplay, type DisplayUnits } from "@/utils/units";

export function resolveDimensionDisplayAs(
  sketch: SketchFeatureParameters | null,
  dimensionId: string,
) {
  if (!sketch) {
    return "";
  }
  const dimension = sketch.dimensions.find(
    (candidate) => candidate.dimension_id === dimensionId,
  );
  return dimension?.display_as ?? "";
}

export function isProjectedCircleDimension(
  sketch: SketchFeatureParameters | null,
  dimensionId: string,
) {
  if (!sketch) {
    return false;
  }
  const dimension = sketch.dimensions.find(
    (candidate) => candidate.dimension_id === dimensionId,
  );
  if (!dimension || dimension.kind !== "circle_radius") {
    return false;
  }
  return sketch.projections.some((projection) =>
    projection.generated_circle_ids.includes(dimension.entity_id),
  );
}

export function dimensionDisplayValue({
  dimension,
  coreValue,
  sketch,
}: {
  dimension: SketchDimensionScene;
  coreValue: number;
  sketch: SketchFeatureParameters | null;
}) {
  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    // Core value is signed (planegcs L2LAngle constraint enforces a
    // directed angle), but users see an unsigned interior angle.
    return Math.abs(coreValue) * (180 / Math.PI);
  }
  if (dimension.kind === "circle_radius") {
    const displayAs = resolveDimensionDisplayAs(sketch, dimension.dimensionId);
    return displayAs === "radius" ? coreValue : coreValue * 2;
  }
  return coreValue;
}

export function dimensionCoreValue({
  dimension,
  displayValue,
  sketch,
}: {
  dimension: SketchDimensionScene;
  displayValue: number;
  sketch: SketchFeatureParameters | null;
}) {
  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    return displayValue * (Math.PI / 180);
  }
  if (dimension.kind === "circle_radius") {
    const displayAs = resolveDimensionDisplayAs(sketch, dimension.dimensionId);
    return displayAs === "radius" ? displayValue : displayValue / 2;
  }
  return displayValue;
}

export function formattedDimensionDisplayValue({
  dimension,
  coreValue,
  sketch,
  displayUnits,
}: {
  dimension: SketchDimensionScene;
  coreValue: number;
  sketch: SketchFeatureParameters | null;
  displayUnits: DisplayUnits;
}) {
  const displayValue = dimensionDisplayValue({
    dimension,
    coreValue,
    sketch,
  });
  const isAngleKind =
    dimension.kind === "angle" || dimension.kind === "line_angle";
  const adjusted = isAngleKind
    ? displayValue
    : mmToDisplay(displayValue, displayUnits);
  return String(parseFloat(adjusted.toFixed(2)));
}
