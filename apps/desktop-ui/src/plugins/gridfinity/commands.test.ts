// @ts-nocheck
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";

import { describe, expect, it } from "vitest";

import {
  makeCreateGridfinityFeatureCommand,
  normalizeGridfinityFeatureParameters,
} from "./commands";
import {
  configToFeatureParameters,
  defaultGridfinityConfig,
  migrateGridfinityConfig,
} from "./defaultConfig";
import type { PluginGeometryOperation } from "../sdk";

function gridfinityOperations(
  config = defaultGridfinityConfig,
): PluginGeometryOperation[] {
  const command = makeCreateGridfinityFeatureCommand(
    configToFeatureParameters(config),
  );
  return (command.payload as { geometry: PluginGeometryOperation[] }).geometry;
}

function closeTo(value: number | undefined, expected: number) {
  return value !== undefined && Math.abs(value - expected) < 1e-9;
}

describe("Gridfinity geometry recipe", () => {
  it("defaults new bins to one compartment", () => {
    expect(defaultGridfinityConfig.configSchemaVersion).toBe(10);
    expect(defaultGridfinityConfig.compartmentsX).toBe(1);
    expect(defaultGridfinityConfig.compartmentsY).toBe(1);
    expect(defaultGridfinityConfig.wallThickness).toBe(1.9);
    expect(defaultGridfinityConfig.floorThickness).toBe(2.25);
    expect(defaultGridfinityConfig.dividerThickness).toBe(1.5);
    expect(defaultGridfinityConfig.labelRidgeWidth).toBe(13);
    expect(defaultGridfinityConfig.lightWallThickness).toBe(1.5);
    expect(defaultGridfinityConfig.multiLabel).toBe(false);
    expect(defaultGridfinityConfig.grabCurve).toBe(true);
    expect(defaultGridfinityConfig.magnetHoleDiameter).toBe(6.5);
    expect(defaultGridfinityConfig.magnetRemovalHoles).toBe(false);
    expect(defaultGridfinityConfig.screwHoles).toBe(true);
    expect(defaultGridfinityConfig.holeyHolesX).toBe(3);
    expect(defaultGridfinityConfig.holeyHolesY).toBe(3);
    expect(defaultGridfinityConfig.holeyHoleShape).toBe("circle");
    expect(defaultGridfinityConfig.holeyHoleSize).toBe(4);
    expect(defaultGridfinityConfig.holeyHoleDepth).toBe(5);
    expect(defaultGridfinityConfig.holeyKeepoutDiameter).toBe(12);
  });

  it("migrates the old two-by-two seed config to one compartment", () => {
    const migrated = migrateGridfinityConfig({
      ...defaultGridfinityConfig,
      configSchemaVersion: 4,
      compartmentsX: 2,
      compartmentsY: 2,
      dividerThickness: undefined,
      labelRidgeWidth: undefined,
      multiLabel: undefined,
      grabCurve: undefined,
      magnetHoleDiameter: undefined,
      magnetRemovalHoles: undefined,
    });

    expect(migrated.configSchemaVersion).toBe(10);
    expect(migrated.compartmentsX).toBe(1);
    expect(migrated.compartmentsY).toBe(1);
    expect(migrated.dividerThickness).toBe(1.5);
    expect(migrated.labelRidgeWidth).toBe(13);
    expect(migrated.lightWallThickness).toBe(1.5);
    expect(migrated.multiLabel).toBe(false);
    expect(migrated.grabCurve).toBe(true);
    expect(migrated.magnetHoleDiameter).toBe(6.5);
    expect(migrated.magnetRemovalHoles).toBe(false);
    expect(migrated.holeyHolesX).toBe(3);
    expect(migrated.holeyHolesY).toBe(3);
    expect(migrated.holeyHoleShape).toBe("circle");
  });

  it("normalizes to upstream grid, height, compartment, and label limits", () => {
    const normalized = normalizeGridfinityFeatureParameters({
      ...configToFeatureParameters(defaultGridfinityConfig),
      gridX: 9,
      gridY: 1,
      gridZ: 1,
      compartmentsX: 99,
      compartmentsY: 99,
      labelRidgeWidth: 99,
    });

    expect(normalized.gridX).toBe(6);
    expect(normalized.gridY).toBe(1);
    expect(normalized.gridZ).toBe(2);
    expect(normalized.compartmentsX).toBe(24);
    expect(normalized.compartmentsY).toBe(4);
    expect(normalized.labelRidgeWidth).toBeCloseTo(4.7125);
  });

  it("supports the upstream solid-bin generator family", () => {
    const parameters = normalizeGridfinityFeatureParameters({
      ...configToFeatureParameters({
        ...defaultGridfinityConfig,
        defaultModelKind: "solid_bin",
        gridX: 1,
        gridY: 1,
        gridZ: 1,
      }),
    });
    const command = makeCreateGridfinityFeatureCommand(parameters);
    const payload = command.payload as {
      feature_type: string;
      display_name: string;
      parameters_summary: string;
      geometry: PluginGeometryOperation[];
    };
    const cavityCuts = payload.geometry.filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        operation.z < 6.9,
    );
    const stackingLipCut = payload.geometry.find(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "tapered_rounded_box",
    );

    expect(parameters.gridZ).toBe(1);
    expect(payload.feature_type).toBe("gridfinity_solid_bin");
    expect(payload.display_name).toBe("Gridfinity Solid Bin");
    expect(payload.parameters_summary).toBe("1 x 1 x 1 solid bin");
    expect(cavityCuts).toHaveLength(0);
    expect(stackingLipCut).toMatchObject({
      width: 37.7,
      depth: 37.7,
      top_width: 41.48,
      top_depth: 41.48,
    });
  });

  it("supports the upstream light-bin generator family", () => {
    const command = makeCreateGridfinityFeatureCommand(
      configToFeatureParameters({
        ...defaultGridfinityConfig,
        defaultModelKind: "light_bin",
        gridX: 1,
        gridY: 1,
        gridZ: 3,
        compartmentsX: 1,
        compartmentsY: 1,
      }),
    );
    const payload = command.payload as {
      feature_type: string;
      display_name: string;
      parameters_summary: string;
      geometry: PluginGeometryOperation[];
    };
    const baseSweep = payload.geometry.find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_rect_profile_sweep",
    );
    const floorRingCut = payload.geometry.find(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        closeTo(operation.z, 4.7) &&
        closeTo(operation.height, 1),
    );
    const labelRidge = payload.geometry.find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "profile_extrude" &&
        operation.profile_plane === "yz",
    );

    expect(payload.feature_type).toBe("gridfinity_light_bin");
    expect(payload.display_name).toBe("Gridfinity Light Bin");
    expect(payload.parameters_summary).toBe("1 x 1 x 3 light bin");
    expect(baseSweep).toMatchObject({
      path_width: 41.5,
      path_depth: 41.5,
      path_radius: 3.75,
      profile_points: [
        { u: 3.5, v: 0 },
        { u: 3.5, v: 3.15 },
        { u: 1.9, v: 4.75 },
        { u: 0, v: 4.75 },
        { u: 2.15, v: 2.6 },
        { u: 2.15, v: 0.8 },
        { u: 2.95, v: 0 },
      ],
    });
    expect(floorRingCut).toMatchObject({
      x: 2.15,
      y: 2.15,
      width: 37.7,
      depth: 37.7,
    });
    expect(labelRidge).toBeDefined();
  });

  it("derives holey-bin grid dimensions from upstream keepout math", () => {
    const parameters = normalizeGridfinityFeatureParameters({
      ...configToFeatureParameters({
        ...defaultGridfinityConfig,
        defaultModelKind: "holey_bin",
        holeyHolesX: 4,
        holeyHolesY: 2,
        holeyHoleDepth: 15,
        holeyKeepoutDiameter: 12,
      }),
      gridX: 1,
      gridY: 1,
      gridZ: 12,
    });

    expect(parameters.gridX).toBe(2);
    expect(parameters.gridY).toBe(1);
    expect(parameters.gridZ).toBe(4);
  });

  it("builds the upstream holey-bin circle hole grid", () => {
    const command = makeCreateGridfinityFeatureCommand(
      configToFeatureParameters({
        ...defaultGridfinityConfig,
        defaultModelKind: "holey_bin",
        holeyHolesX: 3,
        holeyHolesY: 3,
        holeyHoleShape: "circle",
        holeyHoleSize: 4,
        holeyHoleDepth: 5,
        holeyKeepoutDiameter: 12,
      }),
    );
    const payload = command.payload as {
      feature_type: string;
      display_name: string;
      parameters_summary: string;
      geometry: PluginGeometryOperation[];
    };
    const circleHoles = payload.geometry.filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "cylinder" &&
        operation.radius === 2 &&
        closeTo(operation.z, 9),
    );
    const cavityCuts = payload.geometry.filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        operation.z < 13.9,
    );

    expect(payload.feature_type).toBe("gridfinity_holey_bin");
    expect(payload.display_name).toBe("Gridfinity Holey Bin");
    expect(payload.parameters_summary).toBe("3 x 3 circle holes");
    expect(circleHoles).toHaveLength(9);
    expect(circleHoles).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ x: 8.433333333333334, y: 8.433333333333334 }),
        expect.objectContaining({ x: 21, y: 21 }),
        expect.objectContaining({ x: 33.56666666666667, y: 33.56666666666667 }),
      ]),
    );
    expect(cavityCuts).toHaveLength(0);
  });

  it("supports square and hexagonal holey-bin holes without core-specific code", () => {
    const squareGeometry = gridfinityOperations({
      ...defaultGridfinityConfig,
      defaultModelKind: "holey_bin",
      holeyHolesX: 1,
      holeyHolesY: 1,
      holeyHoleShape: "square",
    });
    const hexGeometry = gridfinityOperations({
      ...defaultGridfinityConfig,
      defaultModelKind: "holey_bin",
      holeyHolesX: 1,
      holeyHolesY: 1,
      holeyHoleShape: "hexagon",
    });

    expect(squareGeometry).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          operation: "subtract",
          primitive: "box",
          width: 4,
          depth: 4,
          height: 5.1,
        }),
      ]),
    );
    expect(hexGeometry).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          operation: "subtract",
          primitive: "profile_extrude",
          profile_plane: "xy",
          extrude_z: 5.1,
        }),
      ]),
    );
  });

  it("emits spec-sized repeated base feet for each grid cell", () => {
    const geometry = gridfinityOperations();
    const lowerFoot = geometry.find(
      (operation) =>
        operation.primitive === "tapered_rounded_box" &&
        closeTo(operation.x, 3.2) &&
        closeTo(operation.y, 3.2) &&
        closeTo(operation.z, 0),
    );
    const upperFoot = geometry.find(
      (operation) =>
        operation.primitive === "tapered_rounded_box" &&
        closeTo(operation.x, 2.4) &&
        closeTo(operation.y, 2.4) &&
        closeTo(operation.z, 2.6),
    );

    expect(lowerFoot).toMatchObject({
      width: 35.6,
      depth: 35.6,
      height: 0.8,
      top_width: 37.2,
      top_depth: 37.2,
    });
    expect(upperFoot).toMatchObject({
      width: 37.2,
      depth: 37.2,
      height: 2.15,
      top_width: 41.5,
      top_depth: 41.5,
    });
  });

  it("starts the container cavity above the 4.75 mm base profile", () => {
    const cavityCuts = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        operation.z > 4.75,
    );

    expect(cavityCuts.length).toBeGreaterThan(0);
    expect(Math.min(...cavityCuts.map((operation) => operation.z))).toBeCloseTo(
      7,
    );
  });

  it("creates a chamfered stacking lip receiver from the Gridfinity wall profile", () => {
    const lipAdd = gridfinityOperations().find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_box" &&
        closeTo(operation.x, 0.25) &&
        closeTo(operation.y, 0.25) &&
        closeTo(operation.z, 42) &&
        closeTo(operation.height, 4.4),
    );
    const receiverCut = gridfinityOperations().find(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "rounded_box" &&
        closeTo(operation.x, 2.15) &&
        closeTo(operation.y, 2.15) &&
        closeTo(operation.z, 41.95) &&
        closeTo(operation.width, 79.7) &&
        closeTo(operation.depth, 79.7) &&
        closeTo(operation.height, 4.5),
    );
    const chamferCut = gridfinityOperations().find(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "tapered_rounded_box" &&
        closeTo(operation.x, 2.15) &&
        closeTo(operation.y, 2.15) &&
        closeTo(operation.z, 44.5) &&
        closeTo(operation.width, 79.7) &&
        closeTo(operation.depth, 79.7) &&
        closeTo(operation.height, 1.95),
    );

    expect(lipAdd).toMatchObject({ width: 83.5, depth: 83.5, radius: 3.75 });
    expect(receiverCut).toMatchObject({ radius: 1.85 });
    expect(chamferCut).toMatchObject({
      top_width: 83.48,
      top_depth: 83.48,
      radius: 1.85,
      top_radius: 3.74,
    });
    expect(
      (chamferCut?.top_width ?? 0) - (chamferCut?.width ?? 0),
    ).toBeCloseTo(3.78);
    expect(
      (chamferCut?.top_depth ?? 0) - (chamferCut?.depth ?? 0),
    ).toBeCloseTo(3.78);
    expect(
      (chamferCut?.top_width ?? 0) - (chamferCut?.width ?? 0),
    ).toBeCloseTo(defaultGridfinityConfig.wallThickness * 2 - 0.02);
    expect(
      (chamferCut?.top_depth ?? 0) - (chamferCut?.depth ?? 0),
    ).toBeCloseTo(defaultGridfinityConfig.wallThickness * 2 - 0.02);
  });

  it("adds independent divider walls with gridfinitycreator's default thickness", () => {
    const geometry = gridfinityOperations({
      ...defaultGridfinityConfig,
      compartmentsX: 2,
      compartmentsY: 2,
      grabCurve: false,
      labelTab: false,
    });

    expect(geometry).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          operation: "add",
          primitive: "rounded_box",
          x: 41.25,
          y: 2.15,
          z: 7,
          width: 1.5,
          depth: 79.7,
          height: 35,
        }),
        expect.objectContaining({
          operation: "add",
          primitive: "rounded_box",
          x: 2.15,
          y: 41.25,
          z: 7,
          width: 79.7,
          depth: 1.5,
          height: 35,
        }),
      ]),
    );
  });

  it("places the label ridge as a sloped internal rear shelf", () => {
    const tab = gridfinityOperations().find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "profile_extrude" &&
        operation.profile_plane === "yz" &&
        operation.profile_points?.length === 4,
    );

    expect(closeTo(tab?.x, 2.15)).toBe(true);
    expect(closeTo(tab?.y, 0.25)).toBe(true);
    expect(closeTo(tab?.z, 0)).toBe(true);
    expect(closeTo(tab?.extrude_x, 79.7)).toBe(true);
    expect(tab?.profile_points).toEqual([
      { u: expect.closeTo(68.6), v: expect.closeTo(41.6) },
      { u: expect.closeTo(68.6), v: expect.closeTo(46.4) },
      { u: expect.closeTo(81.6), v: expect.closeTo(46.4) },
      { u: expect.closeTo(76.8), v: expect.closeTo(41.6) },
    ]);
  });

  it("can add label ridges to every compartment row", () => {
    const labelRidges = gridfinityOperations({
      ...defaultGridfinityConfig,
      compartmentsY: 2,
      multiLabel: true,
      grabCurve: false,
    }).filter(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "profile_extrude" &&
        operation.profile_points?.length === 4,
    );

    expect(labelRidges).toHaveLength(2);
    expect(
      labelRidges.map((operation) => (operation.profile_points?.[0]?.u ?? 0) + operation.y),
    ).toEqual([
      expect.closeTo(29),
      expect.closeTo(68.85),
    ]);
  });

  it("adds a sampled grab curve profile inside each row", () => {
    const grabProfiles = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "profile_extrude" &&
        closeTo(operation.x, 2.15) &&
        closeTo(operation.y, 0.25) &&
        closeTo(operation.z, 0) &&
        closeTo(operation.extrude_x, 79.7) &&
        (operation.profile_points?.length ?? 0) > 4,
    );

    expect(grabProfiles).toHaveLength(1);
    expect(grabProfiles[0].profile_points).toHaveLength(14);
    expect(grabProfiles[0].profile_points?.[0]).toEqual({
      u: expect.closeTo(81.6),
      v: expect.closeTo(27.75),
    });
    expect(grabProfiles[0].profile_points?.[1]).toEqual({
      u: expect.closeTo(81.6),
      v: expect.closeTo(7),
    });
    expect(grabProfiles[0].profile_points?.[2]).toEqual({
      u: expect.closeTo(60.85),
      v: expect.closeTo(7),
    });
  });

  it("places magnet holes in all four corners of every grid unit", () => {
    const magnetHoles = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "cylinder" &&
        operation.radius === 3.25 &&
        operation.height === 2,
    );

    expect(magnetHoles).toHaveLength(16);
    expect(magnetHoles).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ x: 10.5, y: 10.5 }),
        expect.objectContaining({ x: 31.5, y: 31.5 }),
        expect.objectContaining({ x: 52.5, y: 52.5 }),
        expect.objectContaining({ x: 73.5, y: 73.5 }),
      ]),
    );
  });

  it("keeps screw holes to the upstream 6 mm base depth", () => {
    const screwHoles = gridfinityOperations().filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "cylinder" &&
        operation.radius === 1.5,
    );

    expect(screwHoles).toHaveLength(16);
    expect(screwHoles.every((operation) => operation.height === 6)).toBe(true);
  });

  it("supports custom magnet diameters and removable magnet notches", () => {
    const holes = gridfinityOperations({
      ...defaultGridfinityConfig,
      gridX: 1,
      gridY: 1,
      magnetHoleDiameter: 8,
      magnetRemovalHoles: true,
      screwHoles: false,
    }).filter(
      (operation) =>
        operation.operation === "subtract" &&
        operation.primitive === "cylinder" &&
        operation.height === 2,
    );

    const magnetHoles = holes.filter((operation) => operation.radius === 4);
    const removalHoles = holes.filter((operation) => operation.radius === 1.75);

    expect(magnetHoles).toHaveLength(4);
    expect(removalHoles).toHaveLength(4);
    expect(removalHoles).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ x: 8.34, y: 8.34 }),
        expect.objectContaining({ x: 33.66, y: 8.34 }),
        expect.objectContaining({ x: 8.34, y: 33.66 }),
        expect.objectContaining({ x: 33.66, y: 33.66 }),
      ]),
    );
  });

  it("builds baseplates from swept gridfinitycreator-style cell profiles", () => {
    const geometry = gridfinityOperations({
      ...defaultGridfinityConfig,
      defaultModelKind: "baseplate",
      gridX: 1,
      gridY: 1,
      magnetHoles: false,
    });
    const filledFloor = geometry.find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_box",
    );
    const sweeps = geometry.filter(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_rect_profile_sweep",
    );

    expect(filledFloor).toBeUndefined();
    expect(sweeps).toHaveLength(1);
    expect(sweeps).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          profile_plane: "yz",
          x: 0,
          y: 0,
          z: 0,
          path_width: 42,
          path_depth: 42,
          path_radius: 4,
          profile_points: [
            { u: 0, v: 0 },
            { u: 0, v: 4.65 },
            { u: 2.25, v: 2.5 },
            { u: 2.25, v: 0.7 },
            { u: 2.85, v: 0 },
          ],
        }),
      ]),
    );
  });

  it("raises the profiled baseplate grid on the weighted base body", () => {
    const geometry = gridfinityOperations({
      ...defaultGridfinityConfig,
      defaultModelKind: "baseplate",
      gridX: 1,
      gridY: 1,
      baseplateStyle: "weighted",
      magnetHoles: false,
    });
    const floor = geometry.find(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_box",
    );
    const sweeps = geometry.filter(
      (operation) =>
        operation.operation === "add" &&
        operation.primitive === "rounded_rect_profile_sweep",
    );

    expect(floor?.height).toBeCloseTo(2.35);
    expect(sweeps).toHaveLength(1);
    expect(sweeps.every((operation) => closeTo(operation.z, 2.35))).toBe(true);
  });

  const corePath = path.resolve(
    process.cwd(),
    "../../native/cad-core/build/cad_core",
  );
  const itWithCore = existsSync(corePath) ? it : it.skip;

  async function expectNativeBody(command: ReturnType<typeof makeCreateGridfinityFeatureCommand>) {
    const core = spawn(corePath);
    const pending = new Map<string, (event: Record<string, unknown>) => void>();
    let stdoutBuffer = "";
    let stderrBuffer = "";

    core.stdout.on("data", (chunk: Buffer) => {
      stdoutBuffer += chunk.toString("utf8");
      const lines = stdoutBuffer.split("\n");
      stdoutBuffer = lines.pop() ?? "";
      for (const line of lines) {
        if (!line.trim()) {
          continue;
        }
        const event = JSON.parse(line) as Record<string, unknown>;
        const id = event.id;
        if (typeof id === "string" && pending.has(id)) {
          pending.get(id)?.(event);
          pending.delete(id);
        }
      }
    });
    core.stderr.on("data", (chunk: Buffer) => {
      stderrBuffer += chunk.toString("utf8");
    });

    function send(nextCommand: Record<string, unknown>) {
      core.stdin.write(`${JSON.stringify(nextCommand)}\n`);
    }

    function waitFor(id: string) {
      return new Promise<Record<string, unknown>>((resolve, reject) => {
        const timeout = setTimeout(() => {
          pending.delete(id);
          reject(new Error(`Timed out waiting for ${id}: ${stderrBuffer}`));
        }, 10_000);
        pending.set(id, (event) => {
          clearTimeout(timeout);
          resolve(event);
        });
      });
    }

    try {
      send({ id: "create-document", type: "create_document", payload: {} });
      await waitFor("create-document");

      command.id = "create-gridfinity";
      send(command as unknown as Record<string, unknown>);
      const created = await waitFor("create-gridfinity");
      expect(created.type).toBe("document_state");

      send({ id: "viewport", type: "get_viewport_state", payload: {} });
      const viewport = await waitFor("viewport");
      const payload = viewport.payload as {
        bodies: unknown[];
        meshes: Array<{ positions: number[]; indices: number[] }>;
      };

      expect(payload.bodies.length).toBeGreaterThan(0);
      expect(payload.meshes.length).toBeGreaterThan(0);
      expect(payload.meshes[0].positions.length).toBeGreaterThan(0);
      expect(payload.meshes[0].indices.length).toBeGreaterThan(0);
    } finally {
      send({ type: "shutdown", payload: {} });
      core.kill();
    }
  }

  itWithCore(
    "builds the generated bin as a native core body",
    async () => {
      const core = spawn(corePath);
      const pending = new Map<string, (event: Record<string, unknown>) => void>();
      let stdoutBuffer = "";
      let stderrBuffer = "";

      core.stdout.on("data", (chunk: Buffer) => {
        stdoutBuffer += chunk.toString("utf8");
        const lines = stdoutBuffer.split("\n");
        stdoutBuffer = lines.pop() ?? "";
        for (const line of lines) {
          if (!line.trim()) {
            continue;
          }
          const event = JSON.parse(line) as Record<string, unknown>;
          const id = event.id;
          if (typeof id === "string" && pending.has(id)) {
            pending.get(id)?.(event);
            pending.delete(id);
          }
        }
      });
      core.stderr.on("data", (chunk: Buffer) => {
        stderrBuffer += chunk.toString("utf8");
      });

      function send(command: Record<string, unknown>) {
        core.stdin.write(`${JSON.stringify(command)}\n`);
      }

      function waitFor(id: string) {
        return new Promise<Record<string, unknown>>((resolve, reject) => {
          const timeout = setTimeout(() => {
            pending.delete(id);
            reject(new Error(`Timed out waiting for ${id}: ${stderrBuffer}`));
          }, 10_000);
          pending.set(id, (event) => {
            clearTimeout(timeout);
            resolve(event);
          });
        });
      }

      try {
        send({ id: "create-document", type: "create_document", payload: {} });
        await waitFor("create-document");

        const createCommand = makeCreateGridfinityFeatureCommand(
          configToFeatureParameters(defaultGridfinityConfig),
        );
        createCommand.id = "create-gridfinity";
        send(createCommand as unknown as Record<string, unknown>);
        const created = await waitFor("create-gridfinity");
        expect(created.type).toBe("document_state");

        send({ id: "viewport", type: "get_viewport_state", payload: {} });
        const viewport = await waitFor("viewport");
        const payload = viewport.payload as {
          bodies: unknown[];
          meshes: Array<{ positions: number[]; indices: number[] }>;
        };

        expect(payload.bodies.length).toBeGreaterThan(0);
        expect(payload.meshes.length).toBeGreaterThan(0);
        expect(payload.meshes[0].positions.length).toBeGreaterThan(0);
        expect(payload.meshes[0].indices.length).toBeGreaterThan(0);
      } finally {
        send({ type: "shutdown", payload: {} });
        core.kill();
      }
    },
    15_000,
  );

  itWithCore(
    "builds the generated baseplate as a native core body",
    async () => {
      await expectNativeBody(
        makeCreateGridfinityFeatureCommand(
          configToFeatureParameters({
            ...defaultGridfinityConfig,
            defaultModelKind: "baseplate",
            gridX: 1,
            gridY: 1,
            magnetHoles: false,
          }),
        ),
      );
    },
    15_000,
  );

  itWithCore(
    "builds the generated solid bin as a native core body",
    async () => {
      await expectNativeBody(
        makeCreateGridfinityFeatureCommand(
          configToFeatureParameters({
            ...defaultGridfinityConfig,
            defaultModelKind: "solid_bin",
            gridX: 1,
            gridY: 1,
            gridZ: 1,
          }),
        ),
      );
    },
    15_000,
  );

  itWithCore(
    "builds the generated holey bin as a native core body",
    async () => {
      await expectNativeBody(
        makeCreateGridfinityFeatureCommand(
          configToFeatureParameters({
            ...defaultGridfinityConfig,
            defaultModelKind: "holey_bin",
            holeyHolesX: 2,
            holeyHolesY: 2,
            holeyHoleShape: "hexagon",
          }),
        ),
      );
    },
    15_000,
  );

  itWithCore(
    "builds the generated light bin as a native core body",
    async () => {
      await expectNativeBody(
        makeCreateGridfinityFeatureCommand(
          configToFeatureParameters({
            ...defaultGridfinityConfig,
            defaultModelKind: "light_bin",
            gridX: 1,
            gridY: 1,
            gridZ: 2,
          }),
        ),
      );
    },
    15_000,
  );
});
