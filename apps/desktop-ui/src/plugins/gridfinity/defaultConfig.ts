import type { GridfinityPluginConfig, GridfinityFeatureParameters } from "./types";

export const defaultGridfinityConfig: GridfinityPluginConfig = {
  configSchemaVersion: 10,
  defaultModelKind: "bin",
  gridX: 2,
  gridY: 2,
  gridZ: 6,
  compartmentsX: 1,
  compartmentsY: 1,
  wallThickness: 1.9,
  floorThickness: 2.25,
  dividerThickness: 1.5,
  labelRidgeWidth: 13,
  lightWallThickness: 1.5,
  stackingLip: true,
  labelTab: true,
  multiLabel: false,
  grabCurve: true,
  magnetHoles: true,
  magnetHoleDiameter: 6.5,
  magnetRemovalHoles: false,
  screwHoles: true,
  holeyHolesX: 3,
  holeyHolesY: 3,
  holeyHoleShape: "circle",
  holeyHoleSize: 4,
  holeyHoleDepth: 5,
  holeyKeepoutDiameter: 12,
  baseplateStyle: "thin",
  drawerFitWidth: 0,
  drawerFitDepth: 0,
};

export function migrateGridfinityConfig(
  config: GridfinityPluginConfig,
): GridfinityPluginConfig {
  const next: GridfinityPluginConfig = {
    ...defaultGridfinityConfig,
    ...config,
    configSchemaVersion: 10,
    dividerThickness: config.dividerThickness ?? 1.5,
    labelRidgeWidth: config.labelRidgeWidth ?? 13,
    lightWallThickness: config.lightWallThickness ?? 1.5,
    multiLabel: config.multiLabel ?? false,
    grabCurve: config.grabCurve ?? true,
    magnetHoleDiameter: config.magnetHoleDiameter ?? 6.5,
    magnetRemovalHoles: config.magnetRemovalHoles ?? false,
    holeyHolesX: config.holeyHolesX ?? 3,
    holeyHolesY: config.holeyHolesY ?? 3,
    holeyHoleShape: config.holeyHoleShape ?? "circle",
    holeyHoleSize: config.holeyHoleSize ?? 4,
    holeyHoleDepth: config.holeyHoleDepth ?? 5,
    holeyKeepoutDiameter: config.holeyKeepoutDiameter ?? 12,
  };

  if (
    config.configSchemaVersion !== 10 &&
    next.gridX === 2 &&
    next.gridY === 2 &&
    next.compartmentsX === 2 &&
    next.compartmentsY === 2
  ) {
    return {
      ...next,
      compartmentsX: 1,
      compartmentsY: 1,
      wallThickness: 1.9,
      floorThickness: 2.25,
    };
  }
  return next;
}

export function configToFeatureParameters(
  config: GridfinityPluginConfig,
): GridfinityFeatureParameters {
  return {
    ...config,
    modelKind: config.defaultModelKind,
  };
}
