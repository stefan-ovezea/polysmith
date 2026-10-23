// @vitest-environment node
// S2: an unknown command must ANSWER, carrying the id it was sent with.
//
// The failure mode this pins: the dispatcher used to fall through silently, so
// the UI's `sendCoreCommand` promise never settled and the app hung with no
// error. The core now emits an `error` event with code UNKNOWN_COMMAND and the
// original id, and keeps serving the next command.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

describe.skipIf(!available)("unknown commands", () => {
  it("answers with an UNKNOWN_COMMAND error carrying the request id", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await expect(
        core.send("this_command_does_not_exist" as never),
      ).rejects.toThrow(/UNKNOWN_COMMAND|Unknown command/);
    } finally {
      await core.close();
    }
  }, 60000);

  it("keeps serving commands after an unknown one", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await expect(core.send("nonsense_command" as never)).rejects.toThrow();
      // The loop is still alive: a real command still gets its reply.
      const reply = await core.send("get_document_state");
      expect(reply.type).toBe("document_state");
    } finally {
      await core.close();
    }
  }, 60000);

  it("a malformed-but-parsable command (missing required field) also answers", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      // `add_sketch_line` without coordinates fails inside the handler; the
      // promise must still settle rather than hang.
      await expect(
        core.send("add_sketch_line", {} as never),
      ).rejects.toThrow();
      const reply = await core.send("get_document_state");
      expect(reply.type).toBe("document_state");
    } finally {
      await core.close();
    }
  }, 60000);
});
