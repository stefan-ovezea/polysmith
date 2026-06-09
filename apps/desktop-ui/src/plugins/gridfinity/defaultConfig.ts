import type { GridfinityPluginConfig, GridfinityFeatureParameters } from "./types";

export const defaultGridfinityConfig: GridfinityPluginConfig = {
  configSchemaVersion: 2,
  defaultModelKind: "bin",
  gridX: 2,
  gridY: 2,
  gridZ: 6,
  compartmentsX: 1,
  compartmentsY: 1,
  wallThickness: 1.6,
  floorThickness: 2.4,
  stackingLip: true,
  labelTab: true,
  magnetHoles: true,
  screwHoles: false,
  baseplateStyle: "thin",
  drawerFitWidth: 0,
  drawerFitDepth: 0,
};

export function configToFeatureParameters(
  config: GridfinityPluginConfig,
): GridfinityFeatureParameters {
  return {
    ...config,
    modelKind: config.defaultModelKind,
  };
}
