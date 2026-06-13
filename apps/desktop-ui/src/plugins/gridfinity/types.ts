export type GridfinityModelKind =
  | "bin"
  | "solid_bin"
  | "holey_bin"
  | "light_bin"
  | "baseplate";

export type GridfinityHoleyBinHoleShape = "circle" | "square" | "hexagon";

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
  dividerThickness: number;
  labelRidgeWidth: number;
  lightWallThickness: number;
  stackingLip: boolean;
  labelTab: boolean;
  multiLabel: boolean;
  grabCurve: boolean;
  magnetHoles: boolean;
  magnetHoleDiameter: number;
  magnetRemovalHoles: boolean;
  screwHoles: boolean;
  holeyHolesX: number;
  holeyHolesY: number;
  holeyHoleShape: GridfinityHoleyBinHoleShape;
  holeyHoleSize: number;
  holeyHoleDepth: number;
  holeyKeepoutDiameter: number;
  baseplateStyle: "thin" | "weighted";
  drawerFitWidth: number;
  drawerFitDepth: number;
}

export interface GridfinityFeatureParameters extends GridfinityPluginConfig {
  modelKind: GridfinityModelKind;
}

export const GRIDFINITY_PLUGIN_ID = "polysmith.gridfinity";
export const GRIDFINITY_BIN_FEATURE_TYPE = "gridfinity_bin";
export const GRIDFINITY_SOLID_BIN_FEATURE_TYPE = "gridfinity_solid_bin";
export const GRIDFINITY_HOLEY_BIN_FEATURE_TYPE = "gridfinity_holey_bin";
export const GRIDFINITY_LIGHT_BIN_FEATURE_TYPE = "gridfinity_light_bin";
export const GRIDFINITY_BASEPLATE_FEATURE_TYPE = "gridfinity_baseplate";
