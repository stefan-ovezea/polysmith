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
  // Cancel restores the SURFACES as well as the extent: the body is derived
  // from the profile ids, so a restore that leaves the edited selection in place
  // brings the old depth back onto the new surfaces.
  updateExtrudeProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
}
