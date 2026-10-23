// @vitest-environment node
// Reference line flavours over the real IPC path: the builder the UI sends,
// executed by the real cad_core binary, then read back through BOTH payloads
// the UI consumes (document_state for the tool panel, viewport_state for the
// renderer). The C++ suite (tests/line_type_test.cpp) owns the geometry and
// profile rules.
//
// Prerequisite: a built core (`pnpm core:build`). Skipped when the binary is
// missing.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchLineCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

async function startSketch(core: CadCoreHarness) {
  await core.send("create_document");
  await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
}

// Sketch edits that leave the dimension list unchanged come back as a
// partial `sketch_entities_changed` event, not a full document_state, so ask
// for the snapshot explicitly (that is what the UI store does anyway).
async function documentAfterEdits(core: CadCoreHarness) {
  await core.send("get_document_state");
  return core.latestDocument();
}

function sketchOf(document: ReturnType<CadCoreHarness["latestDocument"]>) {
  const sketch = document?.feature_history.find(
    (feature) => feature.kind === "sketch",
  );
  return sketch?.sketch_parameters;
}

describe.skipIf(!available)("sketch reference line flavours over IPC", () => {
  it("centerline: flag reaches the document and the viewport, no dimensions", async () => {
    const core = new CadCoreHarness();
    try {
      await startSketch(core);
      const command = makeAddSketchLineCommand(0, 0, 40, 0, false, "centerline");
      expect(command.type).toBe("add_sketch_line");
      expect(command.payload).toMatchObject({ line_type: "centerline" });
      await core.send(command.type, command.payload);

      const parameters = sketchOf(await documentAfterEdits(core));
      expect(parameters?.lines).toHaveLength(1);
      expect(parameters?.lines[0].line_type).toBe("centerline");
      expect(parameters?.lines[0].is_construction).toBe(true);
      // The core must not attach automatic dimensions to reference lines.
      expect(parameters?.dimensions ?? []).toHaveLength(0);

      await core.send("get_viewport_state");
      const viewportLine = core.latestViewport()?.sketch_lines?.[0];
      expect(viewportLine?.line_type).toBe("centerline");
      expect(viewportLine?.is_construction).toBe(true);
    } finally {
      await core.close();
    }
  }, 60000);

  it("infinite: the flavour is stored and the clicked endpoints are kept", async () => {
    const core = new CadCoreHarness();
    try {
      await startSketch(core);
      const command = makeAddSketchLineCommand(1, 2, 11, 2, false, "infinite");
      await core.send(command.type, command.payload);

      const line = sketchOf(await documentAfterEdits(core))?.lines[0];
      expect(line?.line_type).toBe("infinite");
      expect(line?.is_construction).toBe(true);
      // The extension is a viewport-only concern; the document keeps the
      // clicked points so scene bounds / camera framing stay sane.
      expect(line?.start_x).toBeCloseTo(1, 9);
      expect(line?.end_x).toBeCloseTo(11, 9);

      await core.send("get_viewport_state");
      expect(core.latestViewport()?.sketch_lines?.[0]?.line_type).toBe("infinite");
    } finally {
      await core.close();
    }
  }, 60000);

  it("a plain line stays solid and gets its automatic dimensions", async () => {
    const core = new CadCoreHarness();
    try {
      await startSketch(core);
      const command = makeAddSketchLineCommand(0, 0, 40, 30, false);
      // No line_type key at all when the flavour is empty.
      expect(command.payload).not.toHaveProperty("line_type");
      await core.send(command.type, command.payload);

      const parameters = sketchOf(await documentAfterEdits(core));
      expect(parameters?.lines).toHaveLength(1);
      expect(parameters?.lines[0].line_type ?? "").toBe("");
      expect(parameters?.lines[0].is_construction).toBe(false);
      expect(parameters?.dimensions?.some((dim) => dim.kind === "line_length")).toBe(
        true,
      );
    } finally {
      await core.close();
    }
  }, 60000);
});
