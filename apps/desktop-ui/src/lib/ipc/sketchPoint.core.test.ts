// @vitest-environment node
// Standalone sketch point over the real IPC path (parity: T1.4).
//
// The point is a `SketchPoint` record that the core re-emits into the vertex
// table as kind "point" (its id IS the vertex id). It must not change the
// profile set, must deduplicate a repeat click, and must be deletable as an
// entity of its own.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchPointCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface SketchParams {
  points?: Array<{ point_id: string; x: number; y: number }>;
  vertices?: Array<{ vertex_id: string; kind: string; x: number; y: number }>;
  lines?: Array<{ line_id: string; start_vertex_id: string }>;
  constraints?: Array<{ kind: string; target_ids: string[] }>;
}

function sketchOf(document: unknown): SketchParams {
  const state = document as {
    feature_history: Array<{
      kind: string;
      sketch_parameters?: SketchParams & { profiles?: unknown[] };
    }>;
  };
  const sketch = state.feature_history.find((f) => f.kind === "sketch");
  return sketch?.sketch_parameters ?? {};
}

async function documentAfterEdits(core: CadCoreHarness) {
  await core.send("get_document_state");
  return core.latestDocument();
}

describe.skipIf(!available)("add_sketch_point over IPC", () => {
  it("creates a movable point vertex and no profile", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      const command = makeAddSketchPointCommand(12, 8);
      await core.send(command.type, command.payload);
      const document = await documentAfterEdits(core);
      const params = sketchOf(document);

      expect(params.points).toHaveLength(1);
      const pointId = params.points![0].point_id;
      expect(pointId).toMatch(/^vertex-\d+$/);

      const vertex = params.vertices?.find((v) => v.vertex_id === pointId);
      expect(vertex).toBeTruthy();
      expect(vertex!.kind).toBe("point");
      expect(vertex!.x).toBeCloseTo(12, 9);
      expect(vertex!.y).toBeCloseTo(8, 9);

      // The UI renders points from the viewport snapshot, not from the
      // document's vertex table (which omits vertices for non-active sketches).
      await core.send("get_viewport_state");
      const viewport = core.latestViewport() as unknown as {
        sketch_vertices: Array<{
          vertex_id: string;
          kind: string;
          position: { x: number; y: number; z: number };
        }>;
        snap_candidates: Array<{
          kind: string;
          vertex_id: string;
          local_x: number;
          local_y: number;
        }>;
      };
      const rendered = viewport.sketch_vertices.find(
        (entry) => entry.vertex_id === pointId,
      );
      expect(rendered).toBeTruthy();
      expect(rendered!.kind).toBe("point");

      // ...and a drag can SNAP to it: the snap resolver only knows about the
      // candidates in this snapshot, so a missing entry means dragging a line
      // onto a placed point silently ignores it.
      const snap = viewport.snap_candidates.find(
        (candidate) =>
          candidate.kind === "endpoint" && candidate.vertex_id === pointId,
      );
      expect(snap).toBeTruthy();
      expect(snap!.local_x).toBeCloseTo(12, 9);
      expect(snap!.local_y).toBeCloseTo(8, 9);

      const profiles = await core.send("get_sketch_profiles", {
        feature_id: (document as { active_sketch_feature_id: string })
          .active_sketch_feature_id,
      });
      expect((profiles.payload as { profile_count: number }).profile_count).toBe(
        0,
      );
      expect(
        (document as { active_sketch_tool: string }).active_sketch_tool,
      ).toBe("point");
    } finally {
      await core.close();
    }
  }, 60000);

  it("leaves a rectangle's single region untouched", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await core.send("add_sketch_rectangle", {
        start_x: 0,
        start_y: 0,
        end_x: 40,
        end_y: 20,
      });
      await core.send("add_sketch_point", { x: 10, y: 8 });
      const document = await documentAfterEdits(core);
      const params = sketchOf(document);

      expect(params.points).toHaveLength(1);
      // Four corners + the point.
      expect(params.vertices).toHaveLength(5);

      const featureId = (document as { active_sketch_feature_id: string })
        .active_sketch_feature_id;
      const profiles = await core.send("get_sketch_profiles", {
        feature_id: featureId,
      });
      const payload = profiles.payload as {
        profile_count: number;
        profiles: Array<{ kind: string; line_ids: string[] }>;
      };
      expect(payload.profile_count).toBe(1);
      expect(payload.profiles[0].kind).toBe("polygon");
      expect(new Set(payload.profiles[0].line_ids)).toEqual(
        new Set(params.lines!.map((line) => line.line_id)),
      );
    } finally {
      await core.close();
    }
  }, 60000);

  it("a repeat click on the same spot does not stack a second point", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await core.send("add_sketch_point", { x: 5, y: 5 });
      const before = await documentAfterEdits(core);
      await core.send("add_sketch_point", { x: 5.0002, y: 5.0002 });
      const after = await documentAfterEdits(core);

      expect(sketchOf(after).points).toHaveLength(1);
      expect(after!.revision).toBe(before!.revision);
    } finally {
      await core.close();
    }
  }, 60000);

  it("deletes only the point", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await core.send("add_sketch_rectangle", {
        start_x: 0,
        start_y: 0,
        end_x: 40,
        end_y: 20,
      });
      await core.send("add_sketch_point", { x: 10, y: 8 });
      const withPoint = await documentAfterEdits(core);
      const pointId = sketchOf(withPoint).points![0].point_id;

      await core.send("delete_sketch_selection", {
        entity_ids: [],
        point_ids: [pointId],
        profile_ids: [],
      });
      const after = await documentAfterEdits(core);
      const params = sketchOf(after);
      expect(params.points ?? []).toHaveLength(0);
      expect(params.vertices).toHaveLength(4);
      expect(params.lines).toHaveLength(4);
    } finally {
      await core.close();
    }
  }, 60000);

  it("a drop onto the point binds and absorbs it in one undo step", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await core.send("add_sketch_rectangle", {
        start_x: 0,
        start_y: 0,
        end_x: 40,
        end_y: 20,
      });
      await core.send("add_sketch_point", { x: 10, y: 8 });
      const withPoint = await documentAfterEdits(core);
      const pointId = sketchOf(withPoint).points![0].point_id;
      const draggedId = sketchOf(withPoint).lines![0].start_vertex_id;

      await core.send("update_sketch_vertex", {
        vertex_id: draggedId,
        x: 10,
        y: 8,
        bind_to_vertex_id: pointId,
      });
      const bound = await documentAfterEdits(core);
      const params = sketchOf(bound);

      // The point is consumed by the endpoint: no duplicate vertex hides under
      // the line, and the binding is recorded as a coincident constraint.
      expect(params.points ?? []).toHaveLength(0);
      expect(params.vertices).toHaveLength(4);
      expect(params.vertices?.some((v) => v.vertex_id === pointId)).toBe(false);
      const dropped = params.vertices?.find((v) => v.vertex_id === draggedId);
      expect(dropped).toBeTruthy();
      expect(dropped!.x).toBeCloseTo(10, 9);
      expect(dropped!.y).toBeCloseTo(8, 9);
      // The binding is visible as a coincident badge in the viewport (the
      // document payload has no constraints array; that is by design).
      await core.send("get_viewport_state");
      const viewport = core.latestViewport() as unknown as {
        sketch_constraints: Array<{ kind: string }>;
      };
      expect(
        viewport.sketch_constraints.some((c) => c.kind === "coincident"),
      ).toBe(true);

      // ONE undo brings the point back and restores the corner.
      await core.send("undo");
      const undone = sketchOf(await documentAfterEdits(core));
      expect(undone.points).toHaveLength(1);
      const restored = undone.vertices?.find((v) => v.vertex_id === draggedId);
      expect(restored).toBeTruthy();
      expect(restored!.x).toBeCloseTo(0, 9);
      expect(restored!.y).toBeCloseTo(0, 9);
    } finally {
      await core.close();
    }
  }, 60000);
});
