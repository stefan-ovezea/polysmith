// @vitest-environment node
// Every sketch tool the UI can arm must be accepted by `set_sketch_tool`.
//
// The UI's tool vocabulary (types/geometry/sketch.ts `SketchTool`) is wider
// than the core's canonical list used to be: `corner` and `split` were
// missing, so arming either one answered "Unsupported sketch tool: split" on
// every click (reported 2026-10-23). The C++ tool_whitelist_test pins the
// canonical list; this pins the IPC path the UI actually uses.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

const UI_TOOLS = [
  // Shape2D
  "rectangle",
  "circle",
  "polygon",
  "line",
  "arc",
  "fillet",
  "chamfer",
  "corner",
  "extend",
  "offset",
  "ellipse",
  "slot",
  "spline",
  "split",
  "trim",
  // Modal tools outside Shape2D
  "select",
  "project",
  "dimension",
  "move",
  "text",
] as const;

describe.skipIf(!available)("set_sketch_tool vocabulary", () => {
  it("accepts every tool the toolbar can arm", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      for (const tool of UI_TOOLS) {
        await expect(
          core.send("set_sketch_tool", { tool }),
          `tool ${tool}`,
        ).resolves.toBeTruthy();
      }
    } finally {
      await core.close();
    }
  }, 60000);

  it("still rejects an unknown tool", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await expect(
        core.send("set_sketch_tool", { tool: "bogus" }),
      ).rejects.toThrow(/Unsupported sketch tool/);
    } finally {
      await core.close();
    }
  }, 60000);
});
