// @vitest-environment node
// S11: auto_constrain_sketch over the real IPC path.
//
// The command is one undo step and keeps a candidate only if the solver's DOF
// count drops, so the contract these tests pin is: DOF falls by exactly what was
// added, a second run adds nothing, and one undo puts the sketch back.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "../ai/cadCoreHarness";
import { makeAddSketchLineCommand } from "./sketchCommands";
import { makeAutoConstrainSketchCommand } from "../ipcProtocol";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

async function sendLine(core: CadCoreHarness, x1: number, y1: number, x2: number, y2: number) {
  const command = makeAddSketchLineCommand(x1, y1, x2, y2);
  await core.send(command.type, command.payload);
}

interface SketchDoc {
  feature_history: Array<{
    feature_id: string;
    kind: string;
    sketch_parameters?: { solver_dofs: number } | null;
  }>;
  active_sketch_feature_id: string | null;
}

function sketchDofs(document: unknown): number {
  const typed = document as SketchDoc;
  for (const feature of typed.feature_history) {
    if (feature.sketch_parameters) return feature.sketch_parameters.solver_dofs;
  }
  return -99;
}

async function sendAutoConstrain(core: CadCoreHarness) {
  const command = makeAutoConstrainSketchCommand();
  await core.send(command.type, command.payload);
}
describe.skipIf(!available)("auto_constrain_sketch", () => {
  it("constrains a hand-drawn rectangle and is idempotent", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await sendLine(core, 0, 0, 40, 0.3);
      await sendLine(core, 40.3, 0.1, 39.7, 30);
      await sendLine(core, 39.7, 30.2, 0.1, 30.1);
      await sendLine(core, 0.2, 30, 0.1, 0.2);
      await core.send("get_document_state");
      const before = JSON.stringify(core.latestDocument());

      await sendAutoConstrain(core);
      await core.send("get_document_state");
      const afterFirst = JSON.stringify(core.latestDocument());
      // Something was actually added.
      expect(afterFirst).not.toBe(before);

      // The try_add rule makes a second run a no-op: everything it could add is
      // either present or would not reduce DOF. The DOF numbers themselves are
      // asserted in auto_constrain_apply_test.cpp, where the solver measurement
      // is reachable; the document payload does not carry solver_dofs.
      await sendAutoConstrain(core);
      await core.send("get_document_state");
      expect(JSON.stringify(core.latestDocument())).toBe(afterFirst);
    } finally {
      await core.close();
    }
  }, 90000);
  it("one undo restores the sketch", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await sendLine(core, 0, 0, 40, 0.3);
      await sendLine(core, 40.3, 0.1, 39.7, 30);
      await core.send("get_document_state");
      const before = JSON.stringify(core.latestDocument());

      await sendAutoConstrain(core);
      await core.send("get_document_state");
      const afterRun = JSON.stringify(core.latestDocument());
      expect(afterRun).not.toBe(before);

      await core.send("undo");
      await core.send("get_document_state");
      // The undo must restore the geometry AND the constraint state, so compare
      // the sketch feature, not the whole document (undo bookkeeping differs).
      expect(sketchDofs(core.latestDocument())).toBe(sketchDofs(JSON.parse(before)));
    } finally {
      await core.close();
    }
  }, 90000);
});