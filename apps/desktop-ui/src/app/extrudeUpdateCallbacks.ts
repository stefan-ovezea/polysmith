import type { ExtrudeFeatureParameters, ExtrudeMode } from "../types";

export interface ExtrudeUpdateCallbacks {
  updateExtrudeDepth: (featureId: string, depth: number) => Promise<void>;
  updateExtrudeMode: (featureId: string, mode: ExtrudeMode) => Promise<void>;
  updateExtrudeTargetBody: (
    featureId: string,
    targetBodyId: string | null,
  ) => Promise<void>;
  updateExtrudeParameters: (
    featureId: string,
    parameters: ExtrudeFeatureParameters,
  ) => Promise<void>;
}
