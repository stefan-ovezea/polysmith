import type { CoreCommand } from "@/types";
import type { PluginGeometryOperation } from "../sdk";
import {
  GRIDFINITY_BASEPLATE_FEATURE_TYPE,
  GRIDFINITY_BIN_FEATURE_TYPE,
  GRIDFINITY_PLUGIN_ID,
  type GridfinityFeatureParameters,
} from "./types";

const GRID_UNIT_MM = 42;
const GRID_CLEARANCE_MM = 0.5;
const BLOCK_FOOTPRINT_MM = GRID_UNIT_MM - GRID_CLEARANCE_MM;
const OUTER_RADIUS_MM = 3.75;
const INNER_RADIUS_MM = 1.6;
const FOOT_HEIGHT_MM = 4.75;
const FOOT_BOTTOM_SIZE_MM = 35.6;
const FOOT_LOWER_CHAMFER_MM = 0.8;
const FOOT_VERTICAL_MM = 1.8;
const FOOT_UPPER_CHAMFER_MM = 2.15;
const BASEPLATE_THIN_HEIGHT_MM = 4;
const BASEPLATE_WEIGHTED_HEIGHT_MM = 7;
const STACKING_LIP_HEIGHT_MM = 4.4;
const MAGNET_RADIUS_MM = 3.25;
const MAGNET_DEPTH_MM = 2.4;
const SCREW_RADIUS_MM = 1.5;

function featureTypeForParameters(parameters: GridfinityFeatureParameters) {
  return parameters.modelKind === "baseplate"
    ? GRIDFINITY_BASEPLATE_FEATURE_TYPE
    : GRIDFINITY_BIN_FEATURE_TYPE;
}

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(Math.round(value), min), max);
}

export function normalizeGridfinityFeatureParameters(
  parameters: GridfinityFeatureParameters,
): GridfinityFeatureParameters {
  const gridX = clampInteger(parameters.gridX, 1, 24);
  const gridY = clampInteger(parameters.gridY, 1, 24);
  return {
    ...parameters,
    gridX,
    gridY,
    gridZ: clampInteger(parameters.gridZ, 1, 48),
    compartmentsX: clampInteger(parameters.compartmentsX, 1, gridX),
    compartmentsY: clampInteger(parameters.compartmentsY, 1, gridY),
    wallThickness: Math.max(parameters.wallThickness, 0.1),
    floorThickness: Math.max(parameters.floorThickness, 0.1),
  };
}

function op(
  operation: PluginGeometryOperation["operation"],
  primitive: PluginGeometryOperation["primitive"],
  x: number,
  y: number,
  z: number,
  width: number,
  depth: number,
  height: number,
  radius = 0,
): PluginGeometryOperation {
  return { operation, primitive, x, y, z, width, depth, height, radius };
}

function taperedOp(
  x: number,
  y: number,
  z: number,
  width: number,
  depth: number,
  height: number,
  radius: number,
  topWidth: number,
  topDepth: number,
  topRadius: number,
): PluginGeometryOperation {
  return {
    operation: "add",
    primitive: "tapered_rounded_box",
    x,
    y,
    z,
    width,
    depth,
    height,
    radius,
    top_width: topWidth,
    top_depth: topDepth,
    top_radius: topRadius,
  };
}

function outerCornerHoleCenters(width: number, depth: number) {
  const offset = 10.5;
  return [
    [offset, offset],
    [width - offset, offset],
    [offset, depth - offset],
    [width - offset, depth - offset],
  ] as const;
}

function cellCornerHoleCenters(gridX: number, gridY: number) {
  const centers: Array<[number, number]> = [];
  for (let gx = 0; gx < gridX; gx += 1) {
    for (let gy = 0; gy < gridY; gy += 1) {
      const originX = gx * GRID_UNIT_MM;
      const originY = gy * GRID_UNIT_MM;
      centers.push(
        [originX + 10.5, originY + 10.5],
        [originX + 31.5, originY + 10.5],
        [originX + 10.5, originY + 31.5],
        [originX + 31.5, originY + 31.5],
      );
    }
  }
  return centers;
}

function addCornerHoles(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  height: number,
  centers: ReadonlyArray<readonly [number, number]>,
) {
  if (!parameters.magnetHoles && !parameters.screwHoles) {
    return;
  }

  for (const [x, y] of centers) {
    if (parameters.magnetHoles) {
      operations.push(
        op(
          "subtract",
          "cylinder",
          x,
          y,
          -0.05,
          0,
          0,
          MAGNET_DEPTH_MM,
          MAGNET_RADIUS_MM,
        ),
      );
    }
    if (parameters.screwHoles) {
      operations.push(
        op("subtract", "cylinder", x, y, -0.05, 0, 0, height + 0.1, SCREW_RADIUS_MM),
      );
    }
  }
}

function buildBinGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const width = parameters.gridX * GRID_UNIT_MM - GRID_CLEARANCE_MM;
  const depth = parameters.gridY * GRID_UNIT_MM - GRID_CLEARANCE_MM;
  const baseHeight = parameters.gridZ * 7;
  const operations: PluginGeometryOperation[] = [];

  const footMiddleSize = FOOT_BOTTOM_SIZE_MM + FOOT_LOWER_CHAMFER_MM * 2;
  for (let gx = 0; gx < parameters.gridX; gx += 1) {
    for (let gy = 0; gy < parameters.gridY; gy += 1) {
      const cellX = gx * GRID_UNIT_MM;
      const cellY = gy * GRID_UNIT_MM;
      const bottomInset = (GRID_UNIT_MM - FOOT_BOTTOM_SIZE_MM) / 2;
      const middleInset = (GRID_UNIT_MM - footMiddleSize) / 2;

      operations.push(
        taperedOp(
          cellX + bottomInset,
          cellY + bottomInset,
          0,
          FOOT_BOTTOM_SIZE_MM,
          FOOT_BOTTOM_SIZE_MM,
          FOOT_LOWER_CHAMFER_MM,
          INNER_RADIUS_MM,
          footMiddleSize,
          footMiddleSize,
          INNER_RADIUS_MM + FOOT_LOWER_CHAMFER_MM,
        ),
        op(
          "add",
          "rounded_box",
          cellX + middleInset,
          cellY + middleInset,
          FOOT_LOWER_CHAMFER_MM,
          footMiddleSize,
          footMiddleSize,
          FOOT_VERTICAL_MM,
          INNER_RADIUS_MM + FOOT_LOWER_CHAMFER_MM,
        ),
        taperedOp(
          cellX + middleInset,
          cellY + middleInset,
          FOOT_LOWER_CHAMFER_MM + FOOT_VERTICAL_MM,
          footMiddleSize,
          footMiddleSize,
          FOOT_UPPER_CHAMFER_MM,
          INNER_RADIUS_MM + FOOT_LOWER_CHAMFER_MM,
          BLOCK_FOOTPRINT_MM,
          BLOCK_FOOTPRINT_MM,
          OUTER_RADIUS_MM,
        ),
      );
    }
  }

  operations.push(
    op(
      "add",
      "rounded_box",
      GRID_CLEARANCE_MM / 2,
      GRID_CLEARANCE_MM / 2,
      FOOT_HEIGHT_MM - 0.05,
      width,
      depth,
      Math.max(0.05, baseHeight - FOOT_HEIGHT_MM + 0.05),
      OUTER_RADIUS_MM,
    ),
  );

  const usableWidth =
    width - parameters.wallThickness * (parameters.compartmentsX + 1);
  const usableDepth =
    depth - parameters.wallThickness * (parameters.compartmentsY + 1);
  const compartmentWidth = Math.max(
    1,
    usableWidth / parameters.compartmentsX,
  );
  const compartmentDepth = Math.max(
    1,
    usableDepth / parameters.compartmentsY,
  );
  const cavityBottom = Math.min(
    Math.max(FOOT_HEIGHT_MM + 0.1, FOOT_HEIGHT_MM + parameters.floorThickness),
    Math.max(FOOT_HEIGHT_MM + 0.1, baseHeight - 0.4),
  );
  const innerCutHeight = Math.max(0.5, baseHeight - cavityBottom + 0.1);

  for (let gx = 0; gx < parameters.compartmentsX; gx += 1) {
    for (let gy = 0; gy < parameters.compartmentsY; gy += 1) {
      const x =
        GRID_CLEARANCE_MM / 2 +
        parameters.wallThickness +
        gx * (compartmentWidth + parameters.wallThickness);
      const y =
        GRID_CLEARANCE_MM / 2 +
        parameters.wallThickness +
        gy * (compartmentDepth + parameters.wallThickness);
      operations.push(
        op(
          "subtract",
          "rounded_box",
          x,
          y,
          cavityBottom,
          compartmentWidth,
          compartmentDepth,
          innerCutHeight,
          INNER_RADIUS_MM,
        ),
      );
    }
  }

  if (parameters.stackingLip) {
    operations.push(
      taperedOp(
        GRID_CLEARANCE_MM / 2,
        GRID_CLEARANCE_MM / 2,
        baseHeight - 0.25,
        width,
        depth,
        STACKING_LIP_HEIGHT_MM + 0.25,
        OUTER_RADIUS_MM,
        Math.max(1, width - GRID_CLEARANCE_MM),
        Math.max(1, depth - GRID_CLEARANCE_MM),
        Math.max(0.1, OUTER_RADIUS_MM - GRID_CLEARANCE_MM / 2),
      ),
      op(
        "subtract",
        "rounded_box",
        GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
        GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
        baseHeight - 0.35,
        Math.max(1, width - parameters.wallThickness * 2),
        Math.max(1, depth - parameters.wallThickness * 2),
        STACKING_LIP_HEIGHT_MM + 0.55,
        INNER_RADIUS_MM,
      ),
    );
  }

  if (parameters.labelTab) {
    const tabWidth = Math.min(30, Math.max(18, width * 0.35));
    operations.push(
      op(
        "add",
        "rounded_box",
        GRID_CLEARANCE_MM / 2 + width / 2 - tabWidth / 2,
        -5,
        Math.max(cavityBottom, baseHeight - 8),
        tabWidth,
        5.5,
        Math.min(8, baseHeight),
        1.2,
      ),
    );
  }

  addCornerHoles(
    operations,
    parameters,
    baseHeight + (parameters.stackingLip ? STACKING_LIP_HEIGHT_MM : 0),
    cellCornerHoleCenters(parameters.gridX, parameters.gridY),
  );
  return operations;
}

function buildBaseplateGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const width =
    parameters.drawerFitWidth > 0
      ? parameters.drawerFitWidth
      : parameters.gridX * GRID_UNIT_MM;
  const depth =
    parameters.drawerFitDepth > 0
      ? parameters.drawerFitDepth
      : parameters.gridY * GRID_UNIT_MM;
  const height =
    parameters.baseplateStyle === "weighted"
      ? BASEPLATE_WEIGHTED_HEIGHT_MM
      : BASEPLATE_THIN_HEIGHT_MM;
  const operations: PluginGeometryOperation[] = [
    op("add", "rounded_box", 0, 0, 0, width, depth, height, OUTER_RADIUS_MM),
  ];

  const socketInset = 4.8;
  const socketSize = GRID_UNIT_MM - socketInset * 2;
  for (let gx = 0; gx < parameters.gridX; gx += 1) {
    for (let gy = 0; gy < parameters.gridY; gy += 1) {
      operations.push(
        op(
          "subtract",
          "rounded_box",
          gx * GRID_UNIT_MM + socketInset,
          gy * GRID_UNIT_MM + socketInset,
          Math.max(0, height - 2.15),
          socketSize,
          socketSize,
          2.25,
          6.5,
        ),
      );
    }
  }

  addCornerHoles(
    operations,
    parameters,
    height,
    outerCornerHoleCenters(width, depth),
  );
  return operations;
}

function buildGridfinityGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  return parameters.modelKind === "baseplate"
    ? buildBaseplateGeometry(parameters)
    : buildBinGeometry(parameters);
}

function displayNameForParameters(parameters: GridfinityFeatureParameters) {
  return parameters.modelKind === "baseplate"
    ? "Gridfinity Baseplate"
    : "Gridfinity Bin";
}

function summaryForParameters(parameters: GridfinityFeatureParameters) {
  if (parameters.modelKind === "baseplate") {
    return `${parameters.gridX} x ${parameters.gridY} baseplate`;
  }
  return `${parameters.gridX} x ${parameters.gridY} x ${parameters.gridZ} bin, ${parameters.compartmentsX} x ${parameters.compartmentsY} compartments`;
}

export function makeCreateGridfinityFeatureCommand(
  parameters: GridfinityFeatureParameters,
): CoreCommand {
  const normalized = normalizeGridfinityFeatureParameters(parameters);
  const command: CoreCommand = {
    id: crypto.randomUUID(),
    type: "create_plugin_feature",
    payload: {
      plugin_id: GRIDFINITY_PLUGIN_ID,
      feature_type: featureTypeForParameters(normalized),
      schema_version: 1,
      display_name: displayNameForParameters(normalized),
      parameters_summary: summaryForParameters(normalized),
      parameters: normalized,
      geometry: buildGridfinityGeometry(normalized),
    },
  };
  return command;
}

export function makeUpdateGridfinityFeatureCommand(
  featureId: string,
  parameters: GridfinityFeatureParameters,
): CoreCommand {
  const normalized = normalizeGridfinityFeatureParameters(parameters);
  const command: CoreCommand = {
    id: crypto.randomUUID(),
    type: "update_plugin_feature",
    payload: {
      feature_id: featureId,
      plugin_id: GRIDFINITY_PLUGIN_ID,
      feature_type: featureTypeForParameters(normalized),
      schema_version: 1,
      display_name: displayNameForParameters(normalized),
      parameters_summary: summaryForParameters(normalized),
      parameters: normalized,
      geometry: buildGridfinityGeometry(normalized),
    },
  };
  return command;
}

export function makeConfirmGridfinityFeatureCommand(
  featureId: string,
): CoreCommand {
  const command: CoreCommand = {
    id: crypto.randomUUID(),
    type: "confirm_plugin_feature",
    payload: {
      feature_id: featureId,
    },
  };
  return command;
}
