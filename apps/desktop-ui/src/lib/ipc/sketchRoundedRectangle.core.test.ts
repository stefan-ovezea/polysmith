// @vitest-environment node
// Rounded rectangle over the real IPC path: the command builder the UI sends,
// executed by the real cad_core binary. The core semantics are covered by
// native/cad-core/tests/sketch_rounded_rectangle_test.cpp; this pins the part
// the UI owns — the payload shape — plus the serialized state the viewport and
// the tool panel consume.
//
// Prerequisite: a built core (`pnpm core:build`). Skipped when the binary is
// missing; override its location with POLYSMITH_CORE_BIN.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchRoundedRectangleCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

describe.skipIf(!available)("add_sketch_rounded_rectangle over IPC", () => {
  it("creates four sides, four parametric fillets and one closed profile", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });

      const command = makeAddSketchRoundedRectangleCommand(0, 0, 80, 60, 8, false);
      expect(command.type).toBe("add_sketch_rounded_rectangle");
      expect(command.payload).toMatchObject({
        start_x: 0,
        start_y: 0,
        end_x: 80,
        end_y: 60,
        radius: 8,
        is_construction: false,
      });
      await core.send(command.type, command.payload);

      const document = core.latestDocument();
      expect(document).toBeTruthy();
      const sketch = document!.feature_history.find(
        (feature) => feature.kind === "sketch",
      );
      const parameters = sketch?.sketch_parameters;
      expect(parameters).toBeTruthy();
      expect(parameters!.lines).toHaveLength(4);
      expect(parameters!.arcs).toHaveLength(4);
      expect(parameters!.fillets).toHaveLength(4);
      for (const fillet of parameters!.fillets) {
        expect(fillet.radius).toBeCloseTo(8, 9);
      }

      // Complete region set, per the profile-test discipline: the rounded
      // outline is ONE region bounded by the four sides and the four arcs.
      // Profiles are derived and only ship on demand (`get_sketch_profiles`),
      // never in document_state.
      const profilesReply = await core.send("get_sketch_profiles", {
        feature_id: sketch!.feature_id,
      });
      const profilesPayload = profilesReply.payload as {
        profile_count: number;
        profiles: Array<{
          profile_id: string;
          kind: string;
          line_ids: string[];
          points: Array<{ x: number; y: number }>;
          boundary_edges: Array<{
            entity_kind: string;
            center_x: number;
            center_y: number;
            radius: number;
            ccw: boolean;
            start_x: number;
            start_y: number;
            end_x: number;
            end_y: number;
          }>;
        }>;
      };
      expect(profilesPayload.profile_count).toBe(1);
      expect(profilesPayload.profiles).toHaveLength(1);
      // "polygon" is the region TAXONOMY (the enum is polygon/circle/ellipse/
      // spline); it does not mean the boundary is straight. The assertions
      // below are what keep a rounded rectangle from silently degrading into
      // a chamfered polygon.
      expect(profilesPayload.profiles[0].kind).toBe("polygon");
      const expectedBoundary = new Set([
        ...parameters!.lines.map((line) => line.line_id),
        ...parameters!.arcs.map((arc) => arc.arc_id),
      ]);
      expect(new Set(profilesPayload.profiles[0].line_ids)).toEqual(
        expectedBoundary,
      );

      // Exact boundary: line / arc alternating, each arc of the requested
      // radius and a quarter turn — i.e. real fillet arcs, not chords.
      const boundary = profilesPayload.profiles[0].boundary_edges;
      expect(boundary).toHaveLength(8);
      expect(boundary.map((edge) => edge.entity_kind)).toEqual([
        "line",
        "arc",
        "line",
        "arc",
        "line",
        "arc",
        "line",
        "arc",
      ]);
      for (const edge of boundary.filter((e) => e.entity_kind === "arc")) {
        expect(edge.radius).toBeCloseTo(8, 6);
        const startAngle = Math.atan2(
          edge.start_y - edge.center_y,
          edge.start_x - edge.center_x,
        );
        const endAngle = Math.atan2(
          edge.end_y - edge.center_y,
          edge.end_x - edge.center_x,
        );
        let sweep = endAngle - startAngle;
        if (edge.ccw) {
          while (sweep <= 0) sweep += 2 * Math.PI;
        } else {
          while (sweep >= 0) sweep -= 2 * Math.PI;
        }
        expect(Math.abs(sweep)).toBeCloseTo(Math.PI / 2, 6);
      }

      // Area check: a rounded rectangle is the rectangle minus the four
      // corner cut-outs (4 - pi) * r^2. A polygonised/chamfered outline
      // would be off by a visible amount.
      const contour = profilesPayload.profiles[0].points;
      let signedArea = 0;
      for (let i = 0; i < contour.length; i++) {
        const [x1, y1] = [contour[i].x, contour[i].y];
        const next = contour[(i + 1) % contour.length];
        signedArea += x1 * next.y - next.x * y1;
      }
      const area = Math.abs(signedArea) / 2;
      const analytic = 80 * 60 - (4 - Math.PI) * 8 * 8;
      // The sampled loop cuts a hair off each arc (chord error < 0.01 %).
      // A chamfered/polygonised corner would be off by ~1.5 %, so this is a
      // sharp discriminator.
      expect(Math.abs(area - analytic) / analytic).toBeLessThan(1e-3);

      // One undo removes the sides, the arcs and the fillet records — the
      // core composes them into one entry, and the UI relies on that.
      await core.send("undo");
      const undone = core.latestDocument();
      const undoneSketch = undone?.feature_history.find(
        (feature) => feature.kind === "sketch",
      );
      expect(undoneSketch?.sketch_parameters?.lines ?? []).toHaveLength(0);
      expect(undoneSketch?.sketch_parameters?.arcs ?? []).toHaveLength(0);
      expect(undoneSketch?.sketch_parameters?.fillets ?? []).toHaveLength(0);
    } finally {
      await core.close();
    }
  }, 60000);

  it("surfaces the core's refusal when the radius does not fit", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });

      // 60 x 40 rectangle: half the shorter side is 20, so 20 must be refused.
      const command = makeAddSketchRoundedRectangleCommand(0, 0, 60, 40, 20, false);
      await expect(core.send(command.type, command.payload)).rejects.toThrow(
        /half the shorter side/,
      );

      // Nothing was left behind by the aborted undo group.
      const sketch = core
        .latestDocument()
        ?.feature_history.find((feature) => feature.kind === "sketch");
      expect(sketch?.sketch_parameters?.lines ?? []).toHaveLength(0);
      expect(sketch?.sketch_parameters?.fillets ?? []).toHaveLength(0);
    } finally {
      await core.close();
    }
  }, 60000);
});
