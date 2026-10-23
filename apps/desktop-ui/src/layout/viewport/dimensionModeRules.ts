import type { DimensionToolMode } from "@/types";

/**
 * The sketch dimension modes, in dropdown order (S1).
 *
 * The ten drawing-sheet modes that used to sit in this union (baseline, chain,
 * ordinate, jogged_radial, curve_min_max, tidy_up, arrange, flip_arrows, match,
 * dimension_break) were removed: none of them had a sketch implementation, so
 * picking one only produced silent no-ops.
 */
export const DIMENSION_TOOL_MODES: readonly DimensionToolMode[] = [
  "auto",
  "linear",
  "aligned",
  "angular",
  "radius",
  "diameter",
  "arc_length",
];

/** i18n suffix per mode: the label is `toolbar.dimension${suffix}`. */
export const DIMENSION_MODE_LABEL_SUFFIX: Record<DimensionToolMode, string> = {
  auto: "Auto",
  linear: "Linear",
  aligned: "Aligned",
  angular: "Angular",
  radius: "Radius",
  diameter: "Diameter",
  arc_length: "ArcLength",
};

/** A sketch entity kind as reported by the viewport hit test. */
export type DimensionTargetKind =
  | "line"
  | "circle"
  | "arc"
  | "polygon"
  | "spline"
  | string
  | null;

/**
 * Whether a mode may create a dimension from this entity kind.
 *
 * The rules exist so a mode cannot silently produce a different dimension than
 * it promises: before S1, clicking a line while the Radius or Arc length mode
 * was armed fell through to the auto path and quietly created a LINEAR
 * dimension. `auto`, `linear` and `aligned` accept anything (they are
 * multi-entity or drag-driven); the specific modes are strict.
 */
export function dimensionModeAcceptsEntity(
  mode: DimensionToolMode,
  kind: DimensionTargetKind,
): boolean {
  switch (mode) {
    case "auto":
    case "linear":
    case "aligned":
      return true;
    case "angular":
      return kind === "line";
    case "radius":
    case "diameter":
      return kind === "circle" || kind === "arc";
    case "arc_length":
      return kind === "arc";
  }
}

/** i18n key for the one-line hint under the mode dropdown. */
export function dimensionModeHintKey(mode: DimensionToolMode): string {
  switch (mode) {
    case "auto":
      return "viewport.dimensionHintAuto";
    case "linear":
      return "viewport.dimensionHintLinear";
    case "aligned":
      return "viewport.dimensionHintAligned";
    case "angular":
      return "viewport.dimensionHintAngular";
    case "radius":
      return "viewport.dimensionHintRadius";
    case "diameter":
      return "viewport.dimensionHintDiameter";
    case "arc_length":
      return "viewport.dimensionHintArcLength";
  }
}
