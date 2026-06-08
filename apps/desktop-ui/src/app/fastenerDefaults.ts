import { findHoleStandard } from "../lib";
import type { FastenerFeatureParameters } from "../types";
import {
  DEFAULT_FASTENER_DIAMETER,
  DEFAULT_FASTENER_LENGTH,
  DEFAULT_FASTENER_SIZE,
  DEFAULT_FASTENER_THREAD_LENGTH,
} from "./appState";

export function defaultFastenerParameters(): FastenerFeatureParameters {
  const standard = defaultMetricFastenerStandard();
  return {
    standard: "metric",
    size: standardId(standard),
    diameter: standardMajorDiameter(standard),
    minor_diameter: standardMinorDiameter(standard),
    pitch: standardPitch(standard),
    length: DEFAULT_FASTENER_LENGTH,
    thread_length: DEFAULT_FASTENER_THREAD_LENGTH,
    head_type: "socket_head",
    drive_type: "hex_socket",
    thread_representation: "cosmetic",
  };
}

type MetricFastenerStandard = ReturnType<typeof defaultMetricFastenerStandard>;

function defaultMetricFastenerStandard() {
  return findHoleStandard("metric", DEFAULT_FASTENER_SIZE);
}

function standardId(standard: MetricFastenerStandard) {
  return standard?.id ?? DEFAULT_FASTENER_SIZE;
}

function standardMajorDiameter(standard: MetricFastenerStandard) {
  return standard?.majorDiameter ?? DEFAULT_FASTENER_DIAMETER;
}

function standardMinorDiameter(standard: MetricFastenerStandard) {
  return standard?.minorDiameter ?? DEFAULT_FASTENER_DIAMETER * 0.84;
}

function standardPitch(standard: MetricFastenerStandard) {
  return standard?.pitch ?? 0.8;
}
