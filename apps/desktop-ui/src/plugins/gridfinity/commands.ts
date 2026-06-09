import type { CoreCommand } from "@/types";
import type { PluginGeometryOperation, PluginProfilePoint } from "../sdk";
import {
  GRIDFINITY_BASEPLATE_FEATURE_TYPE,
  GRIDFINITY_BIN_FEATURE_TYPE,
  GRIDFINITY_HOLEY_BIN_FEATURE_TYPE,
  GRIDFINITY_LIGHT_BIN_FEATURE_TYPE,
  GRIDFINITY_PLUGIN_ID,
  GRIDFINITY_SOLID_BIN_FEATURE_TYPE,
  type GridfinityFeatureParameters,
  type GridfinityHoleyBinHoleShape,
} from "./types";

const GRID_UNIT_MM = 42;
const GRID_CLEARANCE_MM = 0.5;
const MAX_GRID_UNITS = 6;
const MIN_HEIGHT_UNITS = 2;
const MAX_HEIGHT_UNITS = 12;
const MAX_COMPARTMENTS_PER_GRID_UNIT = 4;
const BLOCK_FOOTPRINT_MM = GRID_UNIT_MM - GRID_CLEARANCE_MM;
const OUTER_RADIUS_MM = 3.75;
const INNER_RADIUS_MM = 1.6;
const FOOT_HEIGHT_MM = 4.75;
const FOOT_BOTTOM_SIZE_MM = 35.6;
const FOOT_MIDDLE_SIZE_MM = 37.2;
const FOOT_LOWER_CHAMFER_MM = 0.8;
const FOOT_VERTICAL_MM = 1.8;
const FOOT_UPPER_CHAMFER_MM = 2.15;
const BASEPLATE_THIN_HEIGHT_MM = 4.65;
const BASEPLATE_WEIGHTED_HEIGHT_MM = 7;
const BASEPLATE_PROFILE_WALL_MM = 2.85;
const BASEPLATE_PROFILE_STEP_MM = 2.25;
const BASEPLATE_PROFILE_MID_Z_MM = 2.5;
const BASEPLATE_PROFILE_FLOOR_Z_MM = 0.7;
const BASEPLATE_CORNER_RADIUS_MM = 4;
const LIGHT_BASE_FLOOR_SIZE_MM = BLOCK_FOOTPRINT_MM - 6.7;
const LIGHT_FLOOR_THICKNESS_MM = 0.9;
const LIGHT_BASE_PROFILE_POINTS: PluginProfilePoint[] = [
  { u: 3.5, v: 0 },
  { u: 3.5, v: 3.15 },
  { u: 1.9, v: FOOT_HEIGHT_MM },
  { u: 0, v: FOOT_HEIGHT_MM },
  { u: 2.15, v: 2.6 },
  { u: 2.15, v: 0.8 },
  { u: 2.95, v: 0 },
];
const STACKING_LIP_HEIGHT_MM = 4.4;
const CHAMFER_EPSILON_MM = 0.01;
const LABEL_TAB_HEIGHT_MM = 4.8;
const DEFAULT_MAGNET_DIAMETER_MM = 6.5;
const MAGNET_DEPTH_MM = 2;
const REMOVABLE_MAGNET_HOLE_OFFSET_MM = 2.16;
const REMOVABLE_MAGNET_HOLE_RADIUS_MM = 1.75;
const SCREW_RADIUS_MM = 1.5;
const SCREW_DEPTH_MM = 6;
const HEIGHT_UNIT_MM = 7;
const MAX_HOLEY_HOLE_COUNT = 24;

interface BinLayout {
  width: number;
  depth: number;
  baseHeight: number;
  topHeight: number;
  internalWidth: number;
  internalDepth: number;
  compartmentPitchX: number;
  compartmentPitchY: number;
  cavityBottom: number;
  compartmentHeight: number;
  innerCutHeight: number;
}

function featureTypeForParameters(parameters: GridfinityFeatureParameters) {
  if (parameters.modelKind === "baseplate") {
    return GRIDFINITY_BASEPLATE_FEATURE_TYPE;
  }
  if (parameters.modelKind === "solid_bin") {
    return GRIDFINITY_SOLID_BIN_FEATURE_TYPE;
  }
  if (parameters.modelKind === "holey_bin") {
    return GRIDFINITY_HOLEY_BIN_FEATURE_TYPE;
  }
  if (parameters.modelKind === "light_bin") {
    return GRIDFINITY_LIGHT_BIN_FEATURE_TYPE;
  }
  return GRIDFINITY_BIN_FEATURE_TYPE;
}

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(Math.round(value), min), max);
}

function normalizeHoleyHoleShape(
  value: unknown,
): GridfinityHoleyBinHoleShape {
  return value === "square" || value === "hexagon" || value === "circle"
    ? value
    : "circle";
}

export function normalizeGridfinityFeatureParameters(
  parameters: GridfinityFeatureParameters,
): GridfinityFeatureParameters {
  const holeyHolesX = clampInteger(
    parameters.holeyHolesX ?? 3,
    1,
    MAX_HOLEY_HOLE_COUNT,
  );
  const holeyHolesY = clampInteger(
    parameters.holeyHolesY ?? 3,
    1,
    MAX_HOLEY_HOLE_COUNT,
  );
  const holeyHoleDepth = Math.max(parameters.holeyHoleDepth ?? 5, 0.1);
  const holeyKeepoutDiameter = Math.max(
    parameters.holeyKeepoutDiameter ?? 12,
    0.1,
  );
  const wallThickness = Math.max(parameters.wallThickness, 0.1);
  const holeyGridX = Math.ceil(
    (holeyHolesX * holeyKeepoutDiameter +
      2 * wallThickness +
      GRID_CLEARANCE_MM) /
      GRID_UNIT_MM,
  );
  const holeyGridY = Math.ceil(
    (holeyHolesY * holeyKeepoutDiameter +
      2 * wallThickness +
      GRID_CLEARANCE_MM) /
      GRID_UNIT_MM,
  );
  const gridX = clampInteger(
    parameters.modelKind === "holey_bin" ? holeyGridX : parameters.gridX,
    1,
    MAX_GRID_UNITS,
  );
  const gridY = clampInteger(
    parameters.modelKind === "holey_bin" ? holeyGridY : parameters.gridY,
    1,
    MAX_GRID_UNITS,
  );
  const minHeightUnits =
    parameters.modelKind === "solid_bin" || parameters.modelKind === "light_bin"
      ? 1
      : MIN_HEIGHT_UNITS;
  const gridZ = clampInteger(
    parameters.modelKind === "holey_bin"
      ? 1 + Math.ceil(holeyHoleDepth / HEIGHT_UNIT_MM)
      : parameters.gridZ,
    minHeightUnits,
    MAX_HEIGHT_UNITS,
  );
  const compartmentsX = clampInteger(
    parameters.compartmentsX,
    1,
    gridX * MAX_COMPARTMENTS_PER_GRID_UNIT,
  );
  const compartmentsY = clampInteger(
    parameters.compartmentsY,
    1,
    gridY * MAX_COMPARTMENTS_PER_GRID_UNIT,
  );
  const depth = gridY * GRID_UNIT_MM - GRID_CLEARANCE_MM;
  const compartmentPitchY =
    Math.max(1, depth - wallThickness * 2) / compartmentsY;
  return {
    ...parameters,
    gridX,
    gridY,
    gridZ,
    compartmentsX,
    compartmentsY,
    wallThickness,
    floorThickness: Math.max(parameters.floorThickness, 0.1),
    dividerThickness: Math.max(parameters.dividerThickness ?? 1.5, 0.1),
    lightWallThickness: Math.max(parameters.lightWallThickness ?? 1.5, 0.1),
    labelRidgeWidth: Math.min(
      Math.max(parameters.labelRidgeWidth ?? 13, 1),
      Math.max(1, compartmentPitchY / 2),
    ),
    multiLabel: parameters.multiLabel ?? false,
    grabCurve: parameters.grabCurve ?? true,
    magnetHoleDiameter: Math.max(
      parameters.magnetHoleDiameter ?? DEFAULT_MAGNET_DIAMETER_MM,
      0.1,
    ),
    magnetRemovalHoles: parameters.magnetRemovalHoles ?? false,
    holeyHolesX,
    holeyHolesY,
    holeyHoleShape: normalizeHoleyHoleShape(parameters.holeyHoleShape),
    holeyHoleSize: Math.min(
      Math.max(parameters.holeyHoleSize ?? 4, 0.1),
      holeyKeepoutDiameter,
    ),
    holeyHoleDepth,
    holeyKeepoutDiameter,
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
  operation: PluginGeometryOperation["operation"] = "add",
  topOffsetX = 0,
  topOffsetY = 0,
): PluginGeometryOperation {
  return {
    operation,
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
    top_offset_x: topOffsetX,
    top_offset_y: topOffsetY,
  };
}

function profileExtrudeOp(
  operation: PluginGeometryOperation["operation"],
  x: number,
  y: number,
  z: number,
  profilePlane: "xy" | "xz" | "yz",
  profilePoints: PluginProfilePoint[],
  extrudeX: number,
  extrudeY: number,
  extrudeZ: number,
): PluginGeometryOperation {
  return {
    operation,
    primitive: "profile_extrude",
    x,
    y,
    z,
    width: 1,
    depth: 1,
    height: 1,
    profile_plane: profilePlane,
    profile_points: profilePoints,
    extrude_x: extrudeX,
    extrude_y: extrudeY,
    extrude_z: extrudeZ,
  };
}

function roundedRectProfileSweepOp(
  x: number,
  y: number,
  z: number,
  pathWidth: number,
  pathDepth: number,
  pathRadius: number,
  profilePoints: PluginProfilePoint[],
): PluginGeometryOperation {
  return {
    operation: "add",
    primitive: "rounded_rect_profile_sweep",
    x,
    y,
    z,
    width: 1,
    depth: 1,
    height: 1,
    profile_plane: "yz",
    profile_points: profilePoints,
    path_width: pathWidth,
    path_depth: pathDepth,
    path_radius: pathRadius,
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

function binLayout(parameters: GridfinityFeatureParameters): BinLayout {
  const width = parameters.gridX * GRID_UNIT_MM - GRID_CLEARANCE_MM;
  const depth = parameters.gridY * GRID_UNIT_MM - GRID_CLEARANCE_MM;
  const baseHeight = parameters.gridZ * HEIGHT_UNIT_MM;
  const internalWidth = Math.max(1, width - parameters.wallThickness * 2);
  const internalDepth = Math.max(1, depth - parameters.wallThickness * 2);
  const compartmentPitchX = internalWidth / parameters.compartmentsX;
  const compartmentPitchY = internalDepth / parameters.compartmentsY;
  const cavityBottom = Math.min(
    Math.max(FOOT_HEIGHT_MM + 0.1, FOOT_HEIGHT_MM + parameters.floorThickness),
    Math.max(FOOT_HEIGHT_MM + 0.1, baseHeight - 0.4),
  );
  const compartmentHeight = Math.max(0.5, baseHeight - cavityBottom);
  const innerCutHeight = compartmentHeight + 0.1;

  return {
    width,
    depth,
    baseHeight,
    topHeight: baseHeight + (parameters.stackingLip ? STACKING_LIP_HEIGHT_MM : 0),
    internalWidth,
    internalDepth,
    compartmentPitchX,
    compartmentPitchY,
    cavityBottom,
    compartmentHeight,
    innerCutHeight,
  };
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

  const magnetRadius = parameters.magnetHoleDiameter / 2;
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
          magnetRadius,
        ),
      );
      if (parameters.magnetRemovalHoles) {
        const cellCenterX =
          Math.floor(x / GRID_UNIT_MM) * GRID_UNIT_MM + GRID_UNIT_MM / 2;
        const cellCenterY =
          Math.floor(y / GRID_UNIT_MM) * GRID_UNIT_MM + GRID_UNIT_MM / 2;
        const directionX = x >= cellCenterX ? 1 : -1;
        const directionY = y >= cellCenterY ? 1 : -1;
        operations.push(
          op(
            "subtract",
            "cylinder",
            x + directionX * REMOVABLE_MAGNET_HOLE_OFFSET_MM,
            y + directionY * REMOVABLE_MAGNET_HOLE_OFFSET_MM,
            -0.05,
            0,
            0,
            MAGNET_DEPTH_MM,
            REMOVABLE_MAGNET_HOLE_RADIUS_MM,
          ),
        );
      }
    }
    if (parameters.screwHoles) {
      operations.push(
        op(
          "subtract",
          "cylinder",
          x,
          y,
          -0.05,
          0,
          0,
          Math.min(height + 0.1, SCREW_DEPTH_MM),
          SCREW_RADIUS_MM,
        ),
      );
    }
  }
}

function addStackingLip(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  width: number,
  depth: number,
  baseHeight: number,
) {
  const outerX = GRID_CLEARANCE_MM / 2;
  const outerY = GRID_CLEARANCE_MM / 2;
  const wall = parameters.wallThickness;
  const innerWidth = Math.max(1, width - wall * 2);
  const innerDepth = Math.max(1, depth - wall * 2);
  const innerTopWidth = Math.max(1, width - CHAMFER_EPSILON_MM * 2);
  const innerTopDepth = Math.max(1, depth - CHAMFER_EPSILON_MM * 2);
  const chamferHeight = Math.min(wall, STACKING_LIP_HEIGHT_MM);

  operations.push(
    op(
      "add",
      "rounded_box",
      outerX,
      outerY,
      baseHeight,
      width,
      depth,
      STACKING_LIP_HEIGHT_MM,
      OUTER_RADIUS_MM,
    ),
    op(
      "subtract",
      "rounded_box",
      outerX + wall,
      outerY + wall,
      baseHeight - 0.05,
      innerWidth,
      innerDepth,
      STACKING_LIP_HEIGHT_MM + 0.1,
      Math.max(0.1, OUTER_RADIUS_MM - wall),
    ),
    taperedOp(
      outerX + wall,
      outerY + wall,
      baseHeight + STACKING_LIP_HEIGHT_MM - chamferHeight,
      innerWidth,
      innerDepth,
      chamferHeight + 0.05,
      Math.max(0.1, OUTER_RADIUS_MM - wall),
      innerTopWidth,
      innerTopDepth,
      Math.max(0.1, OUTER_RADIUS_MM - CHAMFER_EPSILON_MM),
      "subtract",
      0,
      0,
    ),
  );
}

function addLabelTab(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  layout: BinLayout,
) {
  const ridgeDepth = Math.min(
    parameters.labelRidgeWidth,
    Math.max(1, layout.compartmentPitchY / 2),
    layout.internalDepth,
  );
  const ridgeHeight = Math.min(
    layout.compartmentHeight,
    ridgeDepth - CHAMFER_EPSILON_MM,
    LABEL_TAB_HEIGHT_MM,
  );
  if (ridgeDepth <= 0.5 || ridgeHeight <= 0.5) {
    return;
  }

  const rowCount = parameters.multiLabel ? parameters.compartmentsY : 1;
  const rowStart = parameters.multiLabel ? 0 : parameters.compartmentsY - 1;
  const ridgeX = GRID_CLEARANCE_MM / 2 + parameters.wallThickness;
  const ridgeYOrigin = GRID_CLEARANCE_MM / 2;

  for (let row = rowStart; row < rowStart + rowCount; row += 1) {
    const rowStartY =
      parameters.wallThickness +
      (row + 1) * layout.compartmentPitchY -
      ridgeDepth;
    operations.push(
      profileExtrudeOp(
        "add",
        ridgeX,
        ridgeYOrigin,
        0,
        "yz",
        [
          { u: rowStartY, v: layout.topHeight - ridgeHeight },
          { u: rowStartY, v: layout.topHeight },
          { u: rowStartY + ridgeDepth, v: layout.topHeight },
          {
            u: rowStartY + ridgeDepth - ridgeHeight,
            v: layout.topHeight - ridgeHeight,
          },
        ],
        layout.internalWidth,
        0,
        0,
      ),
    );
  }
}

function addDividerWalls(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  layout: BinLayout,
) {
  const originX = GRID_CLEARANCE_MM / 2 + parameters.wallThickness;
  const originY = GRID_CLEARANCE_MM / 2 + parameters.wallThickness;
  const divider = Math.min(
    parameters.dividerThickness,
    Math.max(0.1, layout.compartmentPitchX / 2),
    Math.max(0.1, layout.compartmentPitchY / 2),
  );

  for (let gx = 1; gx < parameters.compartmentsX; gx += 1) {
    operations.push(
      op(
        "add",
        "rounded_box",
        originX + gx * layout.compartmentPitchX - divider / 2,
        originY,
        layout.cavityBottom,
        divider,
        layout.internalDepth,
        layout.compartmentHeight,
        0.2,
      ),
    );
  }

  for (let gy = 1; gy < parameters.compartmentsY; gy += 1) {
    operations.push(
      op(
        "add",
        "rounded_box",
        originX,
        originY + gy * layout.compartmentPitchY - divider / 2,
        layout.cavityBottom,
        layout.internalWidth,
        divider,
        layout.compartmentHeight,
        0.2,
      ),
    );
  }
}

function addGrabCurveApproximation(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  layout: BinLayout,
) {
  const rampDepth = Math.min(
    layout.compartmentHeight,
    layout.compartmentPitchY,
    BLOCK_FOOTPRINT_MM / 2,
  );
  if (rampDepth <= 2) {
    return;
  }

  const originX = GRID_CLEARANCE_MM / 2 + parameters.wallThickness;
  const originY = GRID_CLEARANCE_MM / 2;
  const floorZ = HEIGHT_UNIT_MM;
  const sampleCount = 12;

  for (let row = 0; row < parameters.compartmentsY; row += 1) {
    const rowEnd = parameters.wallThickness + (row + 1) * layout.compartmentPitchY;
    const centerY = rowEnd - rampDepth;
    const centerZ = floorZ + rampDepth;
    const profilePoints: PluginProfilePoint[] = [
      { u: rowEnd, v: floorZ + rampDepth },
      { u: rowEnd, v: floorZ },
      { u: centerY, v: floorZ },
    ];

    for (let sample = 1; sample < sampleCount; sample += 1) {
      const angle = -Math.PI / 2 + (sample / sampleCount) * (Math.PI / 2);
      profilePoints.push({
        u: centerY + rampDepth * Math.cos(angle),
        v: centerZ + rampDepth * Math.sin(angle),
      });
    }

    operations.push(
      profileExtrudeOp(
        "add",
        originX,
        originY,
        0,
        "yz",
        profilePoints,
        layout.internalWidth,
        0,
        0,
      ),
    );
  }
}

function regularPolygonPoints(
  sides: number,
  centerX: number,
  centerY: number,
  radius: number,
): PluginProfilePoint[] {
  const points: PluginProfilePoint[] = [];
  for (let index = 0; index < sides; index += 1) {
    const angle = -Math.PI / 2 + (index / sides) * Math.PI * 2;
    points.push({
      u: centerX + Math.cos(angle) * radius,
      v: centerY + Math.sin(angle) * radius,
    });
  }
  return points;
}

function addHoleyBinHoleGrid(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
  layout: BinLayout,
) {
  const stepX = layout.internalWidth / parameters.holeyHolesX;
  const stepY = layout.internalDepth / parameters.holeyHolesY;
  const offsetX = parameters.wallThickness + stepX / 2;
  const offsetY = parameters.wallThickness + stepY / 2;
  const holeBottom = Math.max(0, layout.baseHeight - parameters.holeyHoleDepth);
  const holeHeight = layout.baseHeight - holeBottom + 0.1;
  const holeRadius = parameters.holeyHoleSize / 2;

  for (let hx = 0; hx < parameters.holeyHolesX; hx += 1) {
    for (let hy = 0; hy < parameters.holeyHolesY; hy += 1) {
      const x = GRID_CLEARANCE_MM / 2 + offsetX + hx * stepX;
      const y = GRID_CLEARANCE_MM / 2 + offsetY + hy * stepY;

      if (parameters.holeyHoleShape === "square") {
        operations.push(
          op(
            "subtract",
            "box",
            x - holeRadius,
            y - holeRadius,
            holeBottom,
            parameters.holeyHoleSize,
            parameters.holeyHoleSize,
            holeHeight,
          ),
        );
      } else if (parameters.holeyHoleShape === "hexagon") {
        operations.push(
          profileExtrudeOp(
            "subtract",
            0,
            0,
            holeBottom,
            "xy",
            regularPolygonPoints(6, x, y, holeRadius),
            0,
            0,
            holeHeight,
          ),
        );
      } else {
        operations.push(
          op(
            "subtract",
            "cylinder",
            x,
            y,
            holeBottom,
            0,
            0,
            holeHeight,
            holeRadius,
          ),
        );
      }
    }
  }
}

function baseplateProfilePoints(direction: 1 | -1): PluginProfilePoint[] {
  return [
    { u: 0, v: 0 },
    { u: 0, v: BASEPLATE_THIN_HEIGHT_MM },
    {
      u: direction * BASEPLATE_PROFILE_STEP_MM,
      v: BASEPLATE_PROFILE_MID_Z_MM,
    },
    {
      u: direction * BASEPLATE_PROFILE_STEP_MM,
      v: BASEPLATE_PROFILE_FLOOR_Z_MM,
    },
    { u: direction * BASEPLATE_PROFILE_WALL_MM, v: 0 },
  ];
}

function addBaseplateCellProfile(
  operations: PluginGeometryOperation[],
  cellX: number,
  cellY: number,
  z: number,
) {
  operations.push(
    roundedRectProfileSweepOp(
      cellX,
      cellY,
      z,
      GRID_UNIT_MM,
      GRID_UNIT_MM,
      BASEPLATE_CORNER_RADIUS_MM,
      baseplateProfilePoints(1),
    ),
  );
}

function addGridfinityBaseFeet(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
) {
  const footMiddleSize = FOOT_MIDDLE_SIZE_MM;
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
}

function addLightBinBaseCells(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
) {
  for (let gx = 0; gx < parameters.gridX; gx += 1) {
    for (let gy = 0; gy < parameters.gridY; gy += 1) {
      const cellX = gx * GRID_UNIT_MM + GRID_CLEARANCE_MM / 2;
      const cellY = gy * GRID_UNIT_MM + GRID_CLEARANCE_MM / 2;
      const floorInset = (GRID_UNIT_MM - LIGHT_BASE_FLOOR_SIZE_MM) / 2;

      operations.push(
        roundedRectProfileSweepOp(
          cellX,
          cellY,
          0,
          BLOCK_FOOTPRINT_MM,
          BLOCK_FOOTPRINT_MM,
          OUTER_RADIUS_MM,
          LIGHT_BASE_PROFILE_POINTS,
        ),
        op(
          "add",
          "rounded_box",
          gx * GRID_UNIT_MM + floorInset,
          gy * GRID_UNIT_MM + floorInset,
          0,
          LIGHT_BASE_FLOOR_SIZE_MM,
          LIGHT_BASE_FLOOR_SIZE_MM,
          parameters.lightWallThickness,
          Math.max(0.1, OUTER_RADIUS_MM - floorInset),
        ),
      );
    }
  }
}

function addLightBinFloorRings(
  operations: PluginGeometryOperation[],
  parameters: GridfinityFeatureParameters,
) {
  const cutoutSize = BLOCK_FOOTPRINT_MM - 2 * parameters.wallThickness;
  const cutoutRadius = Math.max(0.1, OUTER_RADIUS_MM - parameters.wallThickness);

  for (let gx = 0; gx < parameters.gridX; gx += 1) {
    for (let gy = 0; gy < parameters.gridY; gy += 1) {
      const cellX = gx * GRID_UNIT_MM;
      const cellY = gy * GRID_UNIT_MM;
      operations.push(
        op(
          "add",
          "rounded_box",
          cellX,
          cellY,
          FOOT_HEIGHT_MM,
          GRID_UNIT_MM,
          GRID_UNIT_MM,
          LIGHT_FLOOR_THICKNESS_MM,
          OUTER_RADIUS_MM,
        ),
        op(
          "subtract",
          "rounded_box",
          cellX + GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
          cellY + GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
          FOOT_HEIGHT_MM - 0.05,
          cutoutSize,
          cutoutSize,
          LIGHT_FLOOR_THICKNESS_MM + 0.1,
          cutoutRadius,
        ),
      );
    }
  }
}

function buildBinGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const layout = binLayout(parameters);
  const operations: PluginGeometryOperation[] = [];

  addGridfinityBaseFeet(operations, parameters);

  operations.push(
    op(
      "add",
      "rounded_box",
      GRID_CLEARANCE_MM / 2,
      GRID_CLEARANCE_MM / 2,
      FOOT_HEIGHT_MM - 0.05,
      layout.width,
      layout.depth,
      Math.max(0.05, layout.baseHeight - FOOT_HEIGHT_MM + 0.05),
      OUTER_RADIUS_MM,
    ),
  );

  operations.push(
    op(
      "subtract",
      "rounded_box",
      GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
      GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
      layout.cavityBottom,
      layout.internalWidth,
      layout.internalDepth,
      layout.innerCutHeight,
      Math.max(0.1, OUTER_RADIUS_MM - parameters.wallThickness),
    ),
  );

  addDividerWalls(operations, parameters, layout);

  if (parameters.grabCurve) {
    addGrabCurveApproximation(operations, parameters, layout);
  }

  if (parameters.stackingLip) {
    addStackingLip(
      operations,
      parameters,
      layout.width,
      layout.depth,
      layout.baseHeight,
    );
  }

  if (parameters.labelTab) {
    addLabelTab(operations, parameters, layout);
  }

  addCornerHoles(
    operations,
    parameters,
    layout.topHeight,
    cellCornerHoleCenters(parameters.gridX, parameters.gridY),
  );
  return operations;
}

function buildLightBinGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const layout = binLayout(parameters);
  const operations: PluginGeometryOperation[] = [];
  const shellHeight =
    Math.max(0, parameters.gridZ - 1) * HEIGHT_UNIT_MM +
    parameters.floorThickness +
    (parameters.stackingLip ? STACKING_LIP_HEIGHT_MM : 0);

  addLightBinBaseCells(operations, parameters);
  addLightBinFloorRings(operations, parameters);

  if (shellHeight > 0) {
    operations.push(
      op(
        "add",
        "rounded_box",
        GRID_CLEARANCE_MM / 2,
        GRID_CLEARANCE_MM / 2,
        FOOT_HEIGHT_MM,
        layout.width,
        layout.depth,
        shellHeight,
        OUTER_RADIUS_MM,
      ),
      op(
        "subtract",
        "rounded_box",
        GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
        GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
        FOOT_HEIGHT_MM,
        layout.internalWidth,
        layout.internalDepth,
        shellHeight + 0.1,
        Math.max(0.1, OUTER_RADIUS_MM - parameters.wallThickness),
      ),
    );

    if (parameters.stackingLip) {
      const innerBottomWidth = Math.max(1, layout.width - parameters.wallThickness * 2);
      const innerBottomDepth = Math.max(1, layout.depth - parameters.wallThickness * 2);
      operations.push(
        taperedOp(
          GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
          GRID_CLEARANCE_MM / 2 + parameters.wallThickness,
          FOOT_HEIGHT_MM + shellHeight - STACKING_LIP_HEIGHT_MM - 0.1,
          innerBottomWidth,
          innerBottomDepth,
          STACKING_LIP_HEIGHT_MM + 0.2,
          Math.max(0.1, OUTER_RADIUS_MM - parameters.wallThickness),
          Math.max(1, layout.width - CHAMFER_EPSILON_MM * 2),
          Math.max(1, layout.depth - CHAMFER_EPSILON_MM * 2),
          Math.max(0.1, OUTER_RADIUS_MM - CHAMFER_EPSILON_MM),
          "subtract",
        ),
      );
    }
  }

  if (parameters.labelTab) {
    addLabelTab(operations, parameters, layout);
  }

  return operations;
}

function buildSolidBinGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const layout = binLayout(parameters);
  const operations: PluginGeometryOperation[] = [];

  addGridfinityBaseFeet(operations, parameters);

  if (layout.baseHeight > FOOT_HEIGHT_MM) {
    operations.push(
      op(
        "add",
        "rounded_box",
        GRID_CLEARANCE_MM / 2,
        GRID_CLEARANCE_MM / 2,
        FOOT_HEIGHT_MM - 0.05,
        layout.width,
        layout.depth,
        Math.max(0.05, layout.baseHeight - FOOT_HEIGHT_MM + 0.05),
        OUTER_RADIUS_MM,
      ),
    );
  }

  if (parameters.stackingLip && layout.baseHeight > FOOT_HEIGHT_MM) {
    addStackingLip(
      operations,
      parameters,
      layout.width,
      layout.depth,
      layout.baseHeight,
    );
  }

  addCornerHoles(
    operations,
    parameters,
    layout.topHeight,
    cellCornerHoleCenters(parameters.gridX, parameters.gridY),
  );
  return operations;
}

function buildHoleyBinGeometry(
  parameters: GridfinityFeatureParameters,
): PluginGeometryOperation[] {
  const layout = binLayout(parameters);
  const operations: PluginGeometryOperation[] = [];

  addGridfinityBaseFeet(operations, parameters);

  operations.push(
    op(
      "add",
      "rounded_box",
      GRID_CLEARANCE_MM / 2,
      GRID_CLEARANCE_MM / 2,
      FOOT_HEIGHT_MM - 0.05,
      layout.width,
      layout.depth,
      Math.max(0.05, layout.baseHeight - FOOT_HEIGHT_MM + 0.05),
      OUTER_RADIUS_MM,
    ),
  );

  addHoleyBinHoleGrid(operations, parameters, layout);

  if (parameters.stackingLip) {
    addStackingLip(
      operations,
      parameters,
      layout.width,
      layout.depth,
      layout.baseHeight,
    );
  }

  addCornerHoles(
    operations,
    parameters,
    layout.topHeight,
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
  const profileBaseZ = Math.max(0, height - BASEPLATE_THIN_HEIGHT_MM);
  const operations: PluginGeometryOperation[] = [];

  if (profileBaseZ > 0) {
    operations.push(
      op(
        "add",
        "rounded_box",
        0,
        0,
        0,
        width,
        depth,
        profileBaseZ,
        BASEPLATE_CORNER_RADIUS_MM,
      ),
    );
  }

  if (parameters.drawerFitWidth > 0 || parameters.drawerFitDepth > 0) {
    operations.push(
      op(
        "add",
        "rounded_box",
        0,
        0,
        0,
        width,
        depth,
        Math.max(0.25, profileBaseZ),
        BASEPLATE_CORNER_RADIUS_MM,
      ),
    );
  }

  for (let gx = 0; gx < parameters.gridX; gx += 1) {
    for (let gy = 0; gy < parameters.gridY; gy += 1) {
      addBaseplateCellProfile(
        operations,
        gx * GRID_UNIT_MM,
        gy * GRID_UNIT_MM,
        profileBaseZ,
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
  if (parameters.modelKind === "baseplate") {
    return buildBaseplateGeometry(parameters);
  }
  if (parameters.modelKind === "solid_bin") {
    return buildSolidBinGeometry(parameters);
  }
  if (parameters.modelKind === "holey_bin") {
    return buildHoleyBinGeometry(parameters);
  }
  if (parameters.modelKind === "light_bin") {
    return buildLightBinGeometry(parameters);
  }
  return buildBinGeometry(parameters);
}

function displayNameForParameters(parameters: GridfinityFeatureParameters) {
  if (parameters.modelKind === "baseplate") {
    return "Gridfinity Baseplate";
  }
  if (parameters.modelKind === "solid_bin") {
    return "Gridfinity Solid Bin";
  }
  if (parameters.modelKind === "holey_bin") {
    return "Gridfinity Holey Bin";
  }
  if (parameters.modelKind === "light_bin") {
    return "Gridfinity Light Bin";
  }
  return "Gridfinity Bin";
}

function summaryForParameters(parameters: GridfinityFeatureParameters) {
  if (parameters.modelKind === "baseplate") {
    return `${parameters.gridX} x ${parameters.gridY} baseplate`;
  }
  if (parameters.modelKind === "solid_bin") {
    return `${parameters.gridX} x ${parameters.gridY} x ${parameters.gridZ} solid bin`;
  }
  if (parameters.modelKind === "holey_bin") {
    return `${parameters.holeyHolesX} x ${parameters.holeyHolesY} ${parameters.holeyHoleShape} holes`;
  }
  if (parameters.modelKind === "light_bin") {
    return `${parameters.gridX} x ${parameters.gridY} x ${parameters.gridZ} light bin`;
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
