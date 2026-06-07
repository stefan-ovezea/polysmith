import type { HoleFeatureParameters, HoleFit, HoleStandard } from "@/types";

export interface HoleStandardEntry {
  id: string;
  label: string;
  standard: Exclude<HoleStandard, "custom">;
  pitch: number;
  majorDiameter: number;
  minorDiameter: number;
  clearanceDiameter: number;
  tapDrillDiameter: number;
  counterboreDiameter: number;
  counterboreDepth: number;
  countersinkDiameter: number;
  countersinkAngleDegrees: number;
}

const METRIC_HOLE_STANDARDS: HoleStandardEntry[] = [
  metric("M2", 0.4, 2.0, 1.6, 2.4, 1.6, 4.4, 2.0),
  metric("M2.5", 0.45, 2.5, 2.05, 2.9, 2.05, 5.0, 2.5),
  metric("M3", 0.5, 3.0, 2.5, 3.4, 2.5, 6.5, 3.0),
  metric("M4", 0.7, 4.0, 3.3, 4.5, 3.3, 8.0, 4.0),
  metric("M5", 0.8, 5.0, 4.2, 5.5, 4.2, 10.0, 5.0),
  metric("M6", 1.0, 6.0, 5.0, 6.6, 5.0, 11.0, 6.0),
  metric("M8", 1.25, 8.0, 6.8, 9.0, 6.8, 15.0, 8.0),
  metric("M10", 1.5, 10.0, 8.5, 11.0, 8.5, 18.0, 10.0),
  metric("M12", 1.75, 12.0, 10.2, 13.5, 10.2, 20.0, 12.0),
];

const IMPERIAL_HOLE_STANDARDS: HoleStandardEntry[] = [
  imperial("#0-80 UNF", 80, 0.06, 0.052, 0.073, 0.052, 0.14, 0.07),
  imperial("#2-56 UNC", 56, 0.086, 0.07, 0.096, 0.07, 0.17, 0.09),
  imperial("#4-40 UNC", 40, 0.112, 0.089, 0.128, 0.089, 0.22, 0.11),
  imperial("#6-32 UNC", 32, 0.138, 0.1065, 0.1495, 0.1065, 0.26, 0.14),
  imperial("#8-32 UNC", 32, 0.164, 0.136, 0.177, 0.136, 0.31, 0.16),
  imperial("#10-24 UNC", 24, 0.19, 0.1495, 0.201, 0.1495, 0.36, 0.19),
  imperial("#10-32 UNF", 32, 0.19, 0.159, 0.201, 0.159, 0.36, 0.19),
  imperial("1/4-20 UNC", 20, 0.25, 0.201, 0.281, 0.201, 0.438, 0.25),
  imperial("1/4-28 UNF", 28, 0.25, 0.213, 0.281, 0.213, 0.438, 0.25),
  imperial("5/16-18 UNC", 18, 0.3125, 0.257, 0.3438, 0.257, 0.531, 0.31),
  imperial("5/16-24 UNF", 24, 0.3125, 0.272, 0.3438, 0.272, 0.531, 0.31),
  imperial("3/8-16 UNC", 16, 0.375, 0.3125, 0.4063, 0.3125, 0.625, 0.38),
  imperial("3/8-24 UNF", 24, 0.375, 0.332, 0.4063, 0.332, 0.625, 0.38),
  imperial("1/2-13 UNC", 13, 0.5, 0.4219, 0.5313, 0.4219, 0.813, 0.5),
  imperial("1/2-20 UNF", 20, 0.5, 0.4531, 0.5313, 0.4531, 0.813, 0.5),
];

export function holeStandardsForMode(standard: HoleStandard) {
  if (standard === "metric") {
    return METRIC_HOLE_STANDARDS;
  }
  if (standard === "imperial") {
    return IMPERIAL_HOLE_STANDARDS;
  }
  return [];
}

export function findHoleStandard(
  standard: HoleStandard,
  standardSize: string,
) {
  return holeStandardsForMode(standard).find((entry) => entry.id === standardSize);
}

export function applyHoleStandard(
  current: HoleFeatureParameters,
  entry: HoleStandardEntry,
  fit: HoleFit,
): HoleFeatureParameters {
  const threaded = fit === "threaded";
  const diameter =
    fit === "clearance" ? entry.clearanceDiameter : entry.tapDrillDiameter;
  return {
    ...current,
    standard: entry.standard,
    standard_size: entry.id,
    hole_fit: fit,
    diameter,
    counterbore_diameter: entry.counterboreDiameter,
    counterbore_depth: entry.counterboreDepth,
    countersink_diameter: entry.countersinkDiameter,
    countersink_angle_degrees: entry.countersinkAngleDegrees,
    thread_enabled: threaded,
    thread_spec: threaded ? entry.label : "",
    thread_pitch: entry.pitch,
    major_diameter: entry.majorDiameter,
    minor_diameter: entry.minorDiameter,
    thread_depth: threaded ? current.depth : current.thread_depth,
    thread_representation: "cosmetic",
  };
}

function metric(
  label: string,
  pitch: number,
  majorDiameter: number,
  minorDiameter: number,
  clearanceDiameter: number,
  tapDrillDiameter: number,
  counterboreDiameter: number,
  counterboreDepth: number,
): HoleStandardEntry {
  return {
    id: label,
    label,
    standard: "metric",
    pitch,
    majorDiameter,
    minorDiameter,
    clearanceDiameter,
    tapDrillDiameter,
    counterboreDiameter,
    counterboreDepth,
    countersinkDiameter: counterboreDiameter,
    countersinkAngleDegrees: 90,
  };
}

function imperial(
  label: string,
  threadsPerInch: number,
  majorDiameterInch: number,
  minorDiameterInch: number,
  clearanceDiameterInch: number,
  tapDrillDiameterInch: number,
  counterboreDiameterInch: number,
  counterboreDepthInch: number,
): HoleStandardEntry {
  const mm = 25.4;
  return {
    id: label,
    label,
    standard: "imperial",
    pitch: mm / threadsPerInch,
    majorDiameter: majorDiameterInch * mm,
    minorDiameter: minorDiameterInch * mm,
    clearanceDiameter: clearanceDiameterInch * mm,
    tapDrillDiameter: tapDrillDiameterInch * mm,
    counterboreDiameter: counterboreDiameterInch * mm,
    counterboreDepth: counterboreDepthInch * mm,
    countersinkDiameter: counterboreDiameterInch * mm,
    countersinkAngleDegrees: 82,
  };
}
