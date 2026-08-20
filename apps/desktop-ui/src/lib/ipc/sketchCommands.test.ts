// @vitest-environment node
// @ts-nocheck
// Regression for the text-on-path update bug (2026-08-20): the command
// builder whitelisted patch keys and silently dropped path_entity_id /
// path_offset, so path binds reached the core as empty patches. The
// core re-expanded (logging as if it worked) with no error anywhere —
// the glyphs just never moved onto the curve. Spread elements in the
// hook's call site skip excess-property checks, so tsc could not catch
// it either. This test pins the wire format itself.
import { describe, expect, it } from "vitest";

import { makeUpdateSketchTextCommand } from "./sketchCommands";

describe("makeUpdateSketchTextCommand", () => {
  it("forwards path_entity_id and path_offset into the payload", () => {
    const command = makeUpdateSketchTextCommand("text-1", {
      path_entity_id: "line-2",
      path_offset: 3.5,
    });
    expect(command.payload).toMatchObject({
      text_id: "text-1",
      path_entity_id: "line-2",
      path_offset: 3.5,
    });
  });

  it("forwards path_entity_id null (clear path) into the payload", () => {
    const command = makeUpdateSketchTextCommand("text-1", {
      path_entity_id: null,
    });
    expect(command.payload).toHaveProperty("path_entity_id", null);
  });

  it("omits absent path fields so the core merges partial patches", () => {
    const command = makeUpdateSketchTextCommand("text-1", { text: "AB" });
    expect(command.payload).not.toHaveProperty("path_entity_id");
    expect(command.payload).not.toHaveProperty("path_offset");
    expect(command.payload).toMatchObject({ text: "AB" });
  });
});
