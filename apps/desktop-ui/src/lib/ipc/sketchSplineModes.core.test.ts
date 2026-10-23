// @vitest-environment node
// Fit-point splines and degree 5 over the real IPC path (parity T2.1).
//
// mode "fit" interpolates: the curve passes through every clicked point, with
// poles solved at the Greville parameters of the same open-uniform knot vector
// the rest of the pipeline evaluates with. Degree is the model's, capped at 5.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchSplineCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface SplineEntry {
  spline_id: string;
  degree: number;
  pole_vertex_ids: string[];
}

function firstSpline(document: unknown): SplineEntry {
  const state = document as {
    feature_history: Array<{
      kind: string;
      sketch_parameters?: { splines?: SplineEntry[] };
    }>;
  };
  const sketch = state.feature_history.find((f) => f.kind === "sketch");
  return sketch!.sketch_parameters!.splines![0];
}

async function documentAfterEdits(core: CadCoreHarness) {
  await core.send("get_document_state");
  return core.latestDocument();
}

describe.skipIf(!available)("add_sketch_spline modes", () => {
  it("creates a fit spline whose degree defaults to 3", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const fitPoints = [
        { x: 0, y: 0 },
        { x: 3, y: 4 },
        { x: 7, y: 5 },
        { x: 10, y: 0 },
      ];
      const command = makeAddSketchSplineCommand(
        fitPoints,
        /*isConstruction=*/false,
        "fit",
      );
      await core.send(command.type, command.payload);
      const spline = firstSpline(await documentAfterEdits(core));

      expect(spline.degree).toBe(3);
      // One pole per fit point: the interpolant has as many poles as points.
      expect(spline.pole_vertex_ids).toHaveLength(fitPoints.length);
    } finally {
      await core.close();
    }
  }, 60000);

  it("honours an explicit degree of 5 (and clamps it to the point count)", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const fitPoints = [
        { x: 0, y: 0 },
        { x: 2, y: 4 },
        { x: 4, y: 6 },
        { x: 7, y: 6.5 },
        { x: 10, y: 5 },
        { x: 12, y: 2 },
        { x: 14, y: 0 },
      ];
      let command = makeAddSketchSplineCommand(fitPoints, false, "fit", 5);
      await core.send(command.type, command.payload);
      expect(firstSpline(await documentAfterEdits(core)).degree).toBe(5);

      // Two points cannot carry degree 5; the cap is point_count - 1.
      command = makeAddSketchSplineCommand(
        [
          { x: 0, y: 0 },
          { x: 5, y: 5 },
        ],
        false,
        "control",
        5,
      );
      await core.send(command.type, command.payload);
      const state = (await documentAfterEdits(core)) as unknown as {
        feature_history: Array<{
          kind: string;
          sketch_parameters?: { splines?: SplineEntry[] };
        }>;
      };
      const splines = state.feature_history.find((f) => f.kind === "sketch")!
        .sketch_parameters!.splines!;
      expect(splines[1].degree).toBe(1);
    } finally {
      await core.close();
    }
  }, 60000);

  it("keeps control mode as the default and rejects an unknown mode", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const points = [
        { x: 0, y: 0 },
        { x: 4, y: 6 },
        { x: 8, y: 6 },
        { x: 12, y: 0 },
      ];
      let command = makeAddSketchSplineCommand(points, false);
      await core.send(command.type, command.payload);
      expect(firstSpline(await documentAfterEdits(core)).degree).toBe(3);

      command = makeAddSketchSplineCommand(points, false, "bogus" as "fit");
      await expect(
        core.send(command.type, command.payload),
      ).rejects.toThrow(/Unknown sketch spline mode/);
    } finally {
      await core.close();
    }
  }, 60000);
});
