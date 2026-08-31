// @vitest-environment node
// Golden-fragment assertions for the AI prompt builders. Exact text is too
// fragile to pin; these fragments are load-bearing rules the model must see
// for envelope compliance and sketch generation.
import { describe, expect, it } from "vitest";

import type { DocumentState } from "@/types";
import {
  buildAiCadRecoveryPrompt,
  buildAiCadSystemPrompt,
  buildAiCadUserPrompt,
} from "./aiCadPrompt";

describe("buildAiCadSystemPrompt", () => {
  it("contains the envelope contract and core rules", () => {
    const prompt = buildAiCadSystemPrompt();
    expect(prompt).toContain("continue");
    expect(prompt).toContain("commands");
    expect(prompt).toContain("start_sketch_on_plane");
    expect(prompt).toContain("ref-plane-xy");
    expect(prompt).toContain("is_construction");
    expect(prompt).toContain("profile_ids");
    expect(prompt).toContain("Never invent");
    expect(prompt).toContain("extrude_profile");
  });
});

describe("buildAiCadUserPrompt", () => {
  it("embeds the user request verbatim and the state summary", () => {
    const document = {
      document_id: "doc-1",
      active_sketch_feature_id: null,
      feature_history: [],
    } as unknown as DocumentState;
    const prompt = buildAiCadUserPrompt(
      "Create a 60x40 rectangle on XY and extrude 20mm",
      document,
      null,
    );
    expect(prompt).toContain("Create a 60x40 rectangle on XY and extrude 20mm");
    expect(prompt).toContain("Current CAD state");
    expect(prompt).toContain("doc-1");
  });
});

describe("buildAiCadRecoveryPrompt", () => {
  it("contains the failure text and the corrected-envelope instruction", () => {
    const prompt = buildAiCadRecoveryPrompt("add_sketch_rectangle: no active sketch");
    expect(prompt).toContain("add_sketch_rectangle: no active sketch");
    expect(prompt).toContain("corrected JSON envelope");
    expect(prompt).toContain("continue");
  });
});
