// @vitest-environment node
// Tangent, tangent-pair and symmetric constraints over the real IPC path (S1).
// Only the AI/scripting surface had these commands before the toolbar buttons;
// this also pins that each one shows up as a viewport badge (the legibility gap
// D-006 described — a constraint that constrains but cannot be seen).
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import type { CoreCommand } from "@/types";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import {
  makeAddSketchCircleCommand,
  makeAddSketchLineCommand,
  makeSetSketchSymmetricConstraintCommand,
  makeSetSketchTangentConstraintCommand,
  makeSetSketchTangentPairConstraintCommand,
} from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface ConstraintEntry {
  kind: string;
  label: string;
}

interface VertexEntry {
  vertex_id: string;
  position: { x: number; y: number };
}

async function viewport(core: CadCoreHarness) {
  const reply = await core.send("get_viewport_state");
  return reply.payload as {
    sketch_constraints: ConstraintEntry[];
    sketch_vertices: VertexEntry[];
  };
}

async function send(core: CadCoreHarness, command: CoreCommand) {
  await core.send(command.type, command.payload ?? {});
}

describe.skipIf(!available)("tangent / tangent-pair / symmetric", () => {
  it("tangent: a line and a circle get a tangent relation with a T badge", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      await send(core, makeAddSketchLineCommand(-10, 0, 10, 0));
      await send(core, makeAddSketchCircleCommand(0, 12, 3));

      await send(
        core,
        makeSetSketchTangentConstraintCommand("line-1", "circle-1"),
      );

      const state = await viewport(core);
      const tangent = state.sketch_constraints.find((constraint) =>
        constraint.kind.startsWith("tangent"),
      );
      expect(tangent).toBeDefined();
      expect(tangent!.label).toBe("T");
    } finally {
      await core.close();
    }
  }, 60000);

  it("tangent pair: two circles become tangent and the badge appears", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      await send(core, makeAddSketchCircleCommand(0, 0, 3));
      await send(core, makeAddSketchCircleCommand(30, 0, 4));

      await send(
        core,
        makeSetSketchTangentPairConstraintCommand("circle-1", "circle-2"),
      );

      const state = await viewport(core);
      expect(
        state.sketch_constraints.some(
          (constraint) =>
            constraint.kind === "tangent_circle_circle" ||
            constraint.kind === "tangent_arc_arc",
        ),
      ).toBe(true);
    } finally {
      await core.close();
    }
  }, 60000);

  it("symmetric: two points mirror about the axis line", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      // Vertical axis at x = 0.
      await send(core, makeAddSketchLineCommand(0, -10, 0, 10));
      await send(core, makeAddSketchLineCommand(-6, 5, -2, 8));
      await send(core, makeAddSketchLineCommand(4, 5, 2, 8));

      // vertex-3 and vertex-5 are the near endpoints of the two branches.
      await send(
        core,
        makeSetSketchSymmetricConstraintCommand("vertex-3", "vertex-5", "line-1"),
      );

      const state = await viewport(core);
      expect(
        state.sketch_constraints.some((constraint) => constraint.kind === "symmetric"),
      ).toBe(true);

      // The solver mirrors the pair about the axis line (x = 0).
      const byId = new Map(state.sketch_vertices.map((v) => [v.vertex_id, v]));
      const left = byId.get("vertex-3")!;
      const right = byId.get("vertex-5")!;
      expect(right.position.x).toBeCloseTo(-left.position.x, 3);
      expect(right.position.y).toBeCloseTo(left.position.y, 3);
    } finally {
      await core.close();
    }
  }, 60000);
});
