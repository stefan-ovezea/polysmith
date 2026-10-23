// @vitest-environment node
// Collinear and midpoint constraints over the real IPC path (S1: the two
// constraint kinds that were reachable only from AI/scripting before the
// toolbar buttons existed).
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import {
  makeAddSketchLineCommand,
  makeSetSketchCollinearConstraintCommand,
  makeSetSketchMidpointConstraintCommand,
} from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface ConstraintEntry {
  kind: string;
  entity_id: string;
  related_entity_id: string | null;
}

async function constraintKinds(
  core: CadCoreHarness,
): Promise<ConstraintEntry[]> {
  const reply = await core.send("get_viewport_state");
  const view = reply.payload as { sketch_constraints?: ConstraintEntry[] };
  return view.sketch_constraints ?? [];
}

async function sendLine(
  core: CadCoreHarness,
  x1: number,
  y1: number,
  x2: number,
  y2: number,
) {
  const command = makeAddSketchLineCommand(x1, y1, x2, y2);
  await core.send(command.type, command.payload);
}

interface SketchLines {
  feature_history: Array<{
    kind: string;
    sketch_parameters?: {
      lines: Array<{ line_id: string; start_vertex_id: string }>;
    };
  }>;
}

function sketchLines(document: unknown) {
  const state = document as SketchLines;
  return state.feature_history.find((f) => f.kind === "sketch")!
    .sketch_parameters!.lines;
}

describe.skipIf(!available)("collinear and midpoint constraints", () => {
  it("collinear: two lines end up with one collinear constraint", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      await sendLine(core, 0, 0, 10, 0);
      await sendLine(core, 12, 0.5, 20, 0.5);
      await core.send("get_document_state");
      const [first, second] = sketchLines(core.latestDocument()).map(
        (line) => line.line_id,
      );

      const command = makeSetSketchCollinearConstraintCommand(second, first);
      await core.send(command.type, command.payload);

      const collinear = (await constraintKinds(core)).filter(
        (constraint) => constraint.kind === "collinear",
      );
      expect(collinear).toHaveLength(1);
      expect(
        new Set([collinear[0].entity_id, collinear[0].related_entity_id]),
      ).toEqual(new Set([first, second]));
    } finally {
      await core.close();
    }
  }, 60000);

  it("midpoint: a line endpoint is pulled onto the host line's midpoint", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      // Host line from (0,0) to (10,0); its midpoint is (5,0).
      await sendLine(core, 0, 0, 10, 0);
      // A second line starting at (5,4): its START vertex is the one to bind.
      await sendLine(core, 5, 4, 5, 8);
      await core.send("get_document_state");
      const [host, moving] = sketchLines(core.latestDocument());

      const command = makeSetSketchMidpointConstraintCommand(
        moving.start_vertex_id,
        host.line_id,
      );
      await core.send(command.type, command.payload);

      expect(
        (await constraintKinds(core)).some(
          (constraint) => constraint.kind === "midpoint",
        ),
      ).toBe(true);

      // The solver should have moved the bound vertex onto the host midpoint.
      const reply = await core.send("get_viewport_state");
      const view = reply.payload as {
        sketch_vertices: Array<{
          vertex_id: string;
          position: { x: number; y: number };
        }>;
      };
      const vertex = view.sketch_vertices.find(
        (v) => v.vertex_id === moving.start_vertex_id,
      );
      expect(view.sketch_vertices.map((v) => v.vertex_id)).toContain(
        moving.start_vertex_id,
      );
      expect(vertex).toBeDefined();
      expect(vertex!.position.x).toBeCloseTo(5, 3);
      expect(vertex!.position.y).toBeCloseTo(0, 3);
    } finally {
      await core.close();
    }
  }, 60000);
});
