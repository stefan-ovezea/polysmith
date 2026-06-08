import { findHoleStandard, holeStandardsForMode } from "../lib";
import type {
  FastenerFeatureParameters,
  ThreadFeatureParameters,
} from "../types";

type ThreadStandard = ThreadFeatureParameters["standard"];
type FastenerStandard = FastenerFeatureParameters["standard"];

export function threadPatchForStandard(
  standard: ThreadStandard,
): Partial<ThreadFeatureParameters> | null {
  if (standard === "custom") {
    return { standard, size: "" };
  }
  const entry = holeStandardsForMode(standard)[0];
  if (!entry) {
    return null;
  }
  return {
    standard,
    size: entry.id,
    major_diameter: entry.majorDiameter,
    minor_diameter: entry.minorDiameter,
    pitch: entry.pitch,
  };
}

export function threadPatchForSize(
  standard: ThreadStandard,
  size: string,
): Partial<ThreadFeatureParameters> | null {
  const entry = findHoleStandard(standard, size);
  if (!entry) {
    return null;
  }
  return {
    size: entry.id,
    major_diameter: entry.majorDiameter,
    minor_diameter: entry.minorDiameter,
    pitch: entry.pitch,
  };
}

export function fastenerPatchForStandard(
  standard: FastenerStandard,
): Partial<FastenerFeatureParameters> | null {
  if (standard === "custom") {
    return { standard, size: "" };
  }
  const entry = holeStandardsForMode(standard)[0];
  if (!entry) {
    return null;
  }
  return {
    standard,
    size: entry.id,
    diameter: entry.majorDiameter,
    minor_diameter: entry.minorDiameter,
    pitch: entry.pitch,
  };
}

export function fastenerPatchForSize(
  standard: FastenerStandard,
  size: string,
): Partial<FastenerFeatureParameters> | null {
  const entry = findHoleStandard(standard, size);
  if (!entry) {
    return null;
  }
  return {
    size: entry.id,
    diameter: entry.majorDiameter,
    minor_diameter: entry.minorDiameter,
    pitch: entry.pitch,
  };
}
