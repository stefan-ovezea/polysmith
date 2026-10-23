// @vitest-environment node
// S5: get_sketch_diagnostics over the real IPC path.
//
// The sketch panel shows "N open ends / N untied" from this command, so the
// contract that matters is: the event comes back with the feature id, the
// counts, and the pairs a "Tie ends" click would join.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchLineCommand } from "./sketchCommands";
import { makeGetSketchDiagnosticsCommand } from "../ipcProtocol";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface DiagnosticsPayload {
  feature_id: string;
  open_count: number;
  untied_count: number;
  open: Array<{ vertex_id: string; owner_ids: string[] }>;
  untied: Array<{ vertex_a_id: string; vertex_b_id: string; gap: number }>;
}

async function sendLine(
  core: CadCoreHarness,
  x1: number,
  y1: number,
  x2: number,
  y2: number,
) {
  const command = makeAddSketchLineCommand(x1, y1, x2, y2);
  await core.send(command.type, command.payload);
}

async function sketchFeatureId(core: CadCoreHarness): Promise<string> {
  await core.send("get_document_state");
  const document = core.latestDocument() as unknown as {
    active_sketch_feature_id: string | null;
  };
  return document.active_sketch_feature_id ?? "";
}

describe.skipIf(!available)("get_sketch_diagnostics", () => {
  it("reports a 0.02 mm gap as one untied pair and four open ends", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      await sendLine(core, 0, 0, 10, 0);
      await sendLine(core, 10.02, 0, 20, 0);
      const featureId = await sketchFeatureId(core);

      const command = makeGetSketchDiagnosticsCommand(featureId);
      const reply = await core.send(command.type, command.payload);
      expect(reply.type).toBe("sketch_diagnostics");
      const payload = reply.payload as DiagnosticsPayload;

      expect(payload.feature_id).toBe(featureId);
      expect(payload.open_count).toBe(4);
      expect(payload.untied_count).toBe(1);
      expect(payload.untied[0].gap).toBeCloseTo(0.02, 6);
    } finally {
      await core.close();
    }
  }, 60000);

  it("reports a closed rectangle as clean", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", {
        reference_id: "ref-plane-xy",
      });
      await core.send("add_sketch_rectangle", {
        start_x: 0,
        start_y: 0,
        end_x: 40,
        end_y: 30,
      });
      const featureId = await sketchFeatureId(core);

      const command = makeGetSketchDiagnosticsCommand(featureId);
      const reply = await core.send(command.type, command.payload);
      const payload = reply.payload as DiagnosticsPayload;

      expect(payload.open_count).toBe(0);
      expect(payload.untied_count).toBe(0);
      expect(payload.open).toEqual([]);
      expect(payload.untied).toEqual([]);
    } finally {
      await core.close();
    }
  }, 60000);

  it("rejects an unknown feature id instead of hanging", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      const command = makeGetSketchDiagnosticsCommand("feature-does-not-exist");
      await expect(core.send(command.type, command.payload)).rejects.toThrow(
        /FEATURE_NOT_FOUND/,
      );
    } finally {
      await core.close();
    }
  }, 60000);
});
