// @vitest-environment node
// Arc slots over the real IPC path (parity T2.3).
//
// mode "arc" makes the centreline an arc: the core expands it into two
// concentric arcs plus two straight radial end caps, all owned by
// `generated_by = "slot:<id>"`, and the region set stays ONE closed polygon.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchSlotCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

const HALF_PI = Math.PI / 2;

interface SketchParams {
  slots?: Array<{
    slot_id: string;
    mode: string;
    center_vertex_id: string;
    arc_radius: number;
    start_angle: number;
    sweep_angle: number;
  }>;
  lines?: Array<{ line_id: string; generated_by: string | null }>;
  arcs?: Array<{ arc_id: string; generated_by: string | null; radius: number }>;
}

function sketchOf(document: unknown): SketchParams {
  const state = document as {
    feature_history: Array<{ kind: string; sketch_parameters?: SketchParams }>;
  };
  return (
    state.feature_history.find((feature) => feature.kind === "sketch")
      ?.sketch_parameters ?? {}
  );
}

async function documentAfterEdits(core: CadCoreHarness) {
  await core.send("get_document_state");
  return core.latestDocument();
}

describe.skipIf(!available)("arc slots over IPC", () => {
  it("expands to two concentric arcs plus two radial caps and one region", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const command = makeAddSketchSlotCommand(
        0,
        0,
        /*length=*/0,
        /*radius=*/2,
        /*rotation=*/0,
        false,
        {
          mode: "arc",
          arcRadius: 10,
          startAngle: 0,
          sweepAngle: HALF_PI,
        },
      );
      await core.send(command.type, command.payload);
      const document = await documentAfterEdits(core);
      const params = sketchOf(document);

      expect(params.slots).toHaveLength(1);
      const slot = params.slots![0];
      expect(slot.mode).toBe("arc");
      expect(slot.arc_radius).toBeCloseTo(10, 9);
      expect(slot.sweep_angle).toBeCloseTo(HALF_PI, 9);

      // Generated geometry: outer radius 12, inner 8, owned by the slot.
      const generatedArcs = (params.arcs ?? []).filter((arc) =>
        (arc.generated_by ?? "").startsWith(`slot:${slot.slot_id}`),
      );
      const generatedLines = (params.lines ?? []).filter((line) =>
        (line.generated_by ?? "").startsWith(`slot:${slot.slot_id}`),
      );
      expect(generatedArcs).toHaveLength(2);
      expect(generatedLines).toHaveLength(2);
      expect(
        generatedArcs
          .map((arc) => Math.round(arc.radius * 1e6) / 1e6)
          .sort((a, b) => a - b),
      ).toEqual([8, 12]);

      const profiles = await core.send("get_sketch_profiles", {
        feature_id: (document as { active_sketch_feature_id: string })
          .active_sketch_feature_id,
      });
      const payload = profiles.payload as {
        profile_count: number;
        profiles: Array<{ kind: string; line_ids: string[] }>;
      };
      expect(payload.profile_count).toBe(1);
      expect(payload.profiles[0].kind).toBe("polygon");
      expect(new Set(payload.profiles[0].line_ids)).toEqual(
        new Set([
          `arc-${slot.slot_id}-outer`,
          `line-${slot.slot_id}-end`,
          `arc-${slot.slot_id}-inner`,
          `line-${slot.slot_id}-start`,
        ]),
      );
    } finally {
      await core.close();
    }
  }, 60000);

  it("refuses a centreline radius that would invert the inner boundary", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const command = makeAddSketchSlotCommand(0, 0, 0, 2, 0, false, {
        mode: "arc",
        arcRadius: 2,
        startAngle: 0,
        sweepAngle: HALF_PI,
      });
      await expect(
        core.send(command.type, command.payload),
      ).rejects.toThrow(/greater than half the slot width/);
    } finally {
      await core.close();
    }
  }, 60000);

  it("moves the whole slot when its centre vertex is dragged", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const command = makeAddSketchSlotCommand(0, 0, 0, 2, 0, false, {
        mode: "arc",
        arcRadius: 10,
        startAngle: 0,
        sweepAngle: HALF_PI,
      });
      await core.send(command.type, command.payload);
      const first = await documentAfterEdits(core);
      const centre = sketchOf(first).slots![0].center_vertex_id;

      await core.send("update_sketch_vertex", {
        vertex_id: centre,
        x: 5,
        y: 5,
      });
      const moved = sketchOf(await documentAfterEdits(core));
      const slot = moved.slots![0];
      expect(slot.mode).toBe("arc");
      // The record's centre IS the arc centre in arc mode.
      const arcs = (moved.arcs ?? []).filter((arc) =>
        (arc.generated_by ?? "").startsWith(`slot:${slot.slot_id}`),
      );
      expect(arcs).toHaveLength(2);
    } finally {
      await core.close();
    }
  }, 60000);
});
