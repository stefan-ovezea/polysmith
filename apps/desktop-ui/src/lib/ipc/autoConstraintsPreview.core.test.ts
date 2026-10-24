// @vitest-environment node
// S11: preview_auto_constraints over the real IPC path.
//
// The preview is the honest half of auto-constrain: it must report what WOULD be
// added and change nothing. These tests pin that contract - the candidate set,
// the dof passthrough, and that the document is untouched.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makePreviewAutoConstraintsCommand } from "../ipcProtocol";
import { makeAddSketchLineCommand } from "./sketchCommands";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

interface PreviewPayload {
  feature_id: string;
  candidate_count: number;
  candidates: Array<{ kind: string; target_ids: string[]; error: number; reason: string }>;
  current_dofs: number;
}

async function sendLine(core: CadCoreHarness, x1: number, y1: number, x2: number, y2: number) {
  const command = makeAddSketchLineCommand(x1, y1, x2, y2);
  await core.send(command.type, command.payload);
}

async function activeSketchId(core: CadCoreHarness): Promise<string> {
  await core.send("get_document_state");
  const document = core.latestDocument() as unknown as {
    active_sketch_feature_id: string | null;
  };
  return document.active_sketch_feature_id ?? "";
}

describe.skipIf(!available)("preview_auto_constraints", () => {
  it("proposes the axis constraint for a nearly horizontal line", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await sendLine(core, 0, 0, 10, 0.0698);
      const featureId = await activeSketchId(core);

      const command = makePreviewAutoConstraintsCommand({ featureId });
      const reply = await core.send(command.type, command.payload);
      expect(reply.type).toBe("auto_constraints_preview");
      const payload = reply.payload as PreviewPayload;

      expect(payload.feature_id).toBe(featureId);
      expect(payload.candidate_count).toBe(1);
      expect(payload.candidates[0].kind).toBe("horizontal");
      expect(payload.candidates[0].target_ids).toEqual(["line-1"]);
      expect(payload.candidates[0].error).toBeGreaterThan(0.3);
      // The solver has not run on a lone line; -1 is "unknown", not "zero".
      expect(payload.current_dofs).toBe(-1);
    } finally {
      await core.close();
    }
  }, 60000);

  it("changes nothing in the document", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await sendLine(core, 0, 0, 10, 0.0698);
      await core.send("get_document_state");
      const before = JSON.stringify(core.latestDocument());

      const featureId = await activeSketchId(core);
      const command = makePreviewAutoConstraintsCommand({ featureId });
      await core.send(command.type, command.payload);

      await core.send("get_document_state");
      expect(JSON.stringify(core.latestDocument())).toBe(before);
    } finally {
      await core.close();
    }
  }, 60000);

  it("reports a clean sketch as zero candidates", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await sendLine(core, 0, 0, 10, 0);
      const featureId = await activeSketchId(core);
      const command = makePreviewAutoConstraintsCommand({ featureId });
      const reply = await core.send(command.type, command.payload);
      const payload = reply.payload as PreviewPayload;
      expect(payload.candidate_count).toBe(0);
      expect(payload.candidates).toEqual([]);
    } finally {
      await core.close();
    }
  }, 60000);
});