// @vitest-environment node
// Chamfer creation modes over the real IPC path: the command builder the UI
// sends, executed by the real cad_core binary. The C++ suite
// (native/cad-core/tests/sketch_chamfer_test.cpp) owns the geometry proofs;
// this pins the payload shape and what the serialized document shows the
// panel (mode, angle, derived distance).
//
// Prerequisite: a built core (`pnpm core:build`). Skipped when the binary is
// missing; override its location with POLYSMITH_CORE_BIN.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import {
  makeAddSketchChamferCommand,
  makeUpdateSketchChamferCommand,
} from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface CornerIds {
  cornerVertexId: string;
  lineAId: string;
  lineBId: string;
}

async function sketchOnRectangle(core: CadCoreHarness) {
  await core.send("create_document");
  await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
  await core.send("add_sketch_rectangle", {
    start_x: 0,
    start_y: 0,
    end_x: 40,
    end_y: 20,
    is_construction: false,
  });
  const sketch = core
    .latestDocument()!
    .feature_history.find((feature) => feature.kind === "sketch")!;
  const lines = sketch.sketch_parameters!.lines;
  // add_sketch_rectangle order: line-1 bottom, line-2 right, line-3 top,
  // line-4 left. The bottom/right corner is (40, 0); the right/top corner
  // is (40, 20).
  return {
    bottomRight: {
      cornerVertexId: lines[0].end_vertex_id,
      lineAId: lines[0].line_id,
      lineBId: lines[1].line_id,
    },
    topRight: {
      cornerVertexId: lines[1].end_vertex_id,
      lineAId: lines[1].line_id,
      lineBId: lines[2].line_id,
    },
  };
}

function chamferOf(core: CadCoreHarness, index = 0) {
  const sketch = core
    .latestDocument()!
    .feature_history.find((feature) => feature.kind === "sketch")!;
  return sketch.sketch_parameters!.chamfers[index];
}

describe.skipIf(!available)("sketch chamfer modes over IPC", () => {
  it("distance_angle derives the second distance and keeps the angle", async () => {
    const core = new CadCoreHarness();
    try {
      const corners: CornerIds = (await sketchOnRectangle(core)).bottomRight;
      const command = makeAddSketchChamferCommand(
        corners.cornerVertexId,
        corners.lineAId,
        corners.lineBId,
        5,
        0,
        "distance_angle",
        30,
      );
      expect(command.type).toBe("add_sketch_chamfer");
      expect(command.payload).toMatchObject({
        distance_a: 5,
        mode: "distance_angle",
        angle_degrees: 30,
      });
      await core.send(command.type, command.payload);

      const chamfer = chamferOf(core);
      expect(chamfer.mode).toBe("distance_angle");
      expect(chamfer.angle_degrees).toBeCloseTo(30, 9);
      // 90 degree corner: distance_b = distance_a * tan(angle).
      expect(chamfer.distance_b).toBeCloseTo(5 * Math.tan((30 * Math.PI) / 180), 6);

      // A distance-only update keeps the mode and the angle.
      const update = makeUpdateSketchChamferCommand(
        chamfer.chamfer_id,
        4,
        0,
        "distance_angle",
        0,
      );
      await core.send(update.type, update.payload);
      const edited = chamferOf(core);
      expect(edited.mode).toBe("distance_angle");
      expect(edited.angle_degrees).toBeCloseTo(30, 9);
      expect(edited.distance_b).toBeCloseTo(4 * Math.tan((30 * Math.PI) / 180), 6);
    } finally {
      await core.close();
    }
  }, 60000);

  it("equal mode applies one distance to both lines", async () => {
    const core = new CadCoreHarness();
    try {
      const corners: CornerIds = (await sketchOnRectangle(core)).topRight;
      const command = makeAddSketchChamferCommand(
        corners.cornerVertexId,
        corners.lineAId,
        corners.lineBId,
        6,
        99,
        "equal",
        0,
      );
      await core.send(command.type, command.payload);

      const chamfer = chamferOf(core);
      expect(chamfer.mode).toBe("equal");
      expect(chamfer.distance_a).toBeCloseTo(6, 9);
      expect(chamfer.distance_b).toBeCloseTo(6, 9);
    } finally {
      await core.close();
    }
  }, 60000);

  it("refuses an angle that cannot meet the second line", async () => {
    const core = new CadCoreHarness();
    try {
      const corners: CornerIds = (await sketchOnRectangle(core)).bottomRight;
      // 90 degrees is the corner's own angle.
      const command = makeAddSketchChamferCommand(
        corners.cornerVertexId,
        corners.lineAId,
        corners.lineBId,
        5,
        0,
        "distance_angle",
        90,
      );
      await expect(core.send(command.type, command.payload)).rejects.toThrow(
        /angle must be between/,
      );
    } finally {
      await core.close();
    }
  }, 60000);
});
