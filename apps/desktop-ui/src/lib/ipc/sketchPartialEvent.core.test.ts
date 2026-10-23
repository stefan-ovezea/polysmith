// @vitest-environment node
// Which reply a sketch edit gets, end to end through the app handlers.
//
// `sketch_entities_changed` only carries line endpoints and vertex
// coordinates. A command that changes anything else must answer with a full
// `document_state`, or the UI keeps a stale copy — the regression that
// produced "sketch object vertex-N not found" from the planegcs bridge after
// a fillet (the UI's arcs still referenced a vertex the patch had dropped).
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchFilletCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

async function rectangleSketch(core: CadCoreHarness, construction = false) {
  await core.send("create_document");
  await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
  await core.send("add_sketch_rectangle", {
    start_x: 0,
    start_y: 0,
    end_x: 40,
    end_y: 20,
    is_construction: construction,
  });
  // Construction geometry answers with the partial event, so ask for the
  // snapshot explicitly before reading it.
  await core.send("get_document_state");
  const sketch = core
    .latestDocument()
    ?.feature_history.find((feature) => feature.kind === "sketch");
  return sketch?.sketch_parameters;
}

describe.skipIf(!available)("sketch edit reply shape", () => {
  it("a fillet answers with the full document state", async () => {
    const core = new CadCoreHarness();
    try {
      const parameters = await rectangleSketch(core);
      const lines = parameters!.lines;
      const command = makeAddSketchFilletCommand(
        lines[0].end_vertex_id,
        lines[0].line_id,
        lines[1].line_id,
        3,
      );
      const reply = await core.send(command.type, command.payload);
      // The new arc + fillet record are not in the partial payload.
      expect(reply.type).toBe("document_state");
    } finally {
      await core.close();
    }
  }, 60000);

  it("a vertex-only edit on construction geometry keeps the partial reply", async () => {
    const core = new CadCoreHarness();
    try {
      const parameters = await rectangleSketch(core, true);
      const reply = await core.send("update_sketch_vertex", {
        vertex_id: parameters!.lines[0].end_vertex_id,
        x: 39,
        y: 1,
      });
      // Only line endpoints and vertex coordinates changed: the fast path.
      expect(reply.type).toBe("sketch_entities_changed");
    } finally {
      await core.close();
    }
  }, 60000);
});
