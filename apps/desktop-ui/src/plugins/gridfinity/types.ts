export type GridfinityModelKind = "bin" | "baseplate";

export interface GridfinityPluginConfig {
  configSchemaVersion: number;
  defaultModelKind: GridfinityModelKind;
  gridX: number;
  gridY: number;
  gridZ: number;
  compartmentsX: number;
  compartmentsY: number;
  wallThickness: number;
  floorThickness: number;
  stackingLip: boolean;
  labelTab: boolean;
  magnetHoles: boolean;
  screwHoles: boolean;
  baseplateStyle: "thin" | "weighted";
  drawerFitWidth: number;
  drawerFitDepth: number;
}

export interface GridfinityFeatureParameters extends GridfinityPluginConfig {
  modelKind: GridfinityModelKind;
}

export const GRIDFINITY_PLUGIN_ID = "polysmith.gridfinity";
export const GRIDFINITY_BIN_FEATURE_TYPE = "gridfinity_bin";
export const GRIDFINITY_BASEPLATE_FEATURE_TYPE = "gridfinity_baseplate";
