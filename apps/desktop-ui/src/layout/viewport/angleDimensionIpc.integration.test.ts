// @vitest-environment node
// End-to-end regression for the angle-dimension editor value path.
//
// The UI builds the `update_sketch_dimension` payload through
// dimensionCoreValue (display degrees) and the core IPC handler converts
// degrees→radians once.  When the UI converted too, a typed 45° arrived
// at the core as 0.79° and the geometry collapsed.  This drives the real
// cad_core binary with the same payload builder the editor uses so the
// whole chain is covered.
//
// Prerequisite: a built core (`pnpm core:build`). Skipped when the binary
// is missing; override its location with POLYSMITH_CORE_BIN.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "@/lib/ai/cadCoreHarness";
import type { SketchDimensionScene } from "@/types";

import { dimensionCoreValue } from "./dimensionValueDisplay";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

const rad = (degrees: number) => (degrees * Math.PI) / 180;
const deg = (radians: number) => (radians * 180) / Math.PI;

function editorScene(
  kind: SketchDimensionScene["kind"],
  dimensionId: string,
): SketchDimensionScene {
  return {
    dimensionId,
    planeId: "ref-plane-xy",
    kind,
    entityId: "line-1",
    label: "",
    rawValue: 0,
    unitSuffix: "",
    isSelected: true,
    anchorStart: [0, 0, 0],
    anchorEnd: [0, 0, 0],
    dimensionStart: [0, 0, 0],
    dimensionEnd: [0, 0, 0],
    labelPosition: [0, 0, 0],
  };
}

function activeSketch(core: CadCoreHarness) {
  const document = core.latestDocument();
  expect(document).toBeTruthy();
  const sketch = document!.feature_history.find(
    (feature) => feature.kind === "sketch" && feature.sketch_parameters,
  );
  expect(sketch).toBeTruthy();
  return sketch!.sketch_parameters!;
}

function lineAngleDegrees(
  sketch: NonNullable<ReturnType<typeof activeSketch>>,
  lineId: string,
): number {
  const line = sketch.lines.find((candidate) => candidate.line_id === lineId);
  expect(line).toBeTruthy();
  return deg(Math.atan2(line!.end_y - line!.start_y, line!.end_x - line!.start_x));
}

describe.skipIf(!available)("angle dimension editor IPC round-trip", () => {
  it("commits a typed 45° as 45° and normalises a reflex target", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      // Two non-axis lines sharing the origin: 20° (length 80) and
      // 70° (length 50), so no H/V inference constrains the solve.
      await core.send("add_sketch_line", {
        start_x: 0,
        start_y: 0,
        end_x: 80 * Math.cos(rad(20)),
        end_y: 80 * Math.sin(rad(20)),
      });
      await core.send("add_sketch_line", {
        start_x: 0,
        start_y: 0,
        end_x: 50 * Math.cos(rad(70)),
        end_y: 50 * Math.sin(rad(70)),
      });

      const first = activeSketch(core).lines[0].line_id;
      const second = activeSketch(core).lines[1].line_id;
      await core.send("add_sketch_angle_dimension", {
        first_line_id: first,
        second_line_id: second,
      });

      const created = activeSketch(core).dimensions.find(
        (dimension) => dimension.kind === "angle",
      );
      expect(created).toBeTruthy();
      expect(deg(created!.value)).toBeCloseTo(50, 6);

      // What the dimension editor submits for a typed "45".
      const payload = dimensionCoreValue({
        dimension: editorScene("angle", created!.dimension_id),
        displayValue: 45,
        sketch: null,
      });
      expect(payload).toBe(45);

      await core.send("update_sketch_dimension", {
        dimension_id: created!.dimension_id,
        value: payload,
      });

      let sketch = activeSketch(core);
      expect(lineAngleDegrees(sketch, first)).toBeCloseTo(20, 6);
      expect(lineAngleDegrees(sketch, second)).toBeCloseTo(65, 6);
      let dimension = sketch.dimensions.find(
        (candidate) => candidate.dimension_id === created!.dimension_id,
      );
      expect(deg(dimension!.value)).toBeCloseTo(45, 6);
      expect(dimension!.driven ?? false).toBe(false);

      // A reflex input has no directed-angle representation; the core
      // clamps it to the included angle instead of flipping the sketch.
      const reflexPayload = dimensionCoreValue({
        dimension: editorScene("angle", created!.dimension_id),
        displayValue: 300,
        sketch: null,
      });
      await core.send("update_sketch_dimension", {
        dimension_id: created!.dimension_id,
        value: reflexPayload,
      });

      sketch = activeSketch(core);
      expect(lineAngleDegrees(sketch, first)).toBeCloseTo(20, 6);
      expect(lineAngleDegrees(sketch, second)).toBeCloseTo(80, 6);
      dimension = sketch.dimensions.find(
        (candidate) => candidate.dimension_id === created!.dimension_id,
      );
      expect(deg(dimension!.value)).toBeCloseTo(60, 6);
      expect(dimension!.driven ?? false).toBe(false);
    } finally {
      await core.close();
    }
  }, 30_000);
});
