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
  // circle_radius dimensions (and every other length kind) already
  // carry their DISPLAYED value over IPC — the core emits the diameter
  // for diameter-mode circle dimensions and the radius for radius mode
  // (feature_to_payload_sketch_dimension_entries.inc), and the update
  // handler converts back on the way in. Converting again here doubled
  // or halved every circle edit, so this passes through.
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
  // circle_radius: send the displayed value as-is; the core's
  // update_sketch_dimension handler halves diameter-mode values when it
  // stores the radius.
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
