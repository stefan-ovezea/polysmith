// @vitest-environment node
// Harness smoke test: drives the REAL cad_core binary through a scripted
// rectangle -> extrude sequence (no Ollama) and verifies the driver plus the
// TS profile-oracle port against the serialized state.
//
// Prerequisite: a built core (`pnpm core:build`). Skipped when the binary is
// missing; override its location with POLYSMITH_CORE_BIN.
import { existsSync } from "node:fs";

import { describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "./cadCoreHarness";
import {
  assertProfilesMatch,
  expectBodyCount,
  expectExtrudeDepth,
  expectRevisionAtLeast,
  featureKinds,
} from "./profileAssertions";

const CORE_EXE = coreBinaryPath();
const available = existsSync(CORE_EXE);

if (!available) {
  // eslint-disable-next-line no-console
  console.warn(
    `[smoke] core binary not found at ${CORE_EXE} — run "pnpm core:build" first.`,
  );
}

describe.skipIf(!available)("cad core harness smoke (scripted, no Ollama)", () => {
  it("drives rectangle -> extrude and asserts the serialized state", async () => {
    const core = new CadCoreHarness();
    try {
      await core.send("create_document");
      await core.send("start_sketch_on_plane", { reference_id: "ref-plane-xy" });
      await core.send("add_sketch_rectangle", {
        start_x: 0,
        start_y: 0,
        end_x: 60,
        end_y: 40,
        is_construction: false,
      });

      const document = core.latestDocument();
      expect(document).toBeTruthy();
      const sketch = document!.feature_history.find(
        (feature) => feature.kind === "sketch",
      );
      expect(sketch?.sketch_parameters?.profiles).toHaveLength(1);

      // Complete-region-set assertion, per the profile-test discipline.
      assertProfilesMatch(document!, [
        { kind: "polygon", boundary_count: 4, has_source_circle_id: false },
      ]);

      const profileId = sketch!.sketch_parameters!.profiles[0].profile_id;
      await core.send("extrude_profile", {
        profile_ids: [profileId],
        depth: 20,
        mode: "new_body",
      });
      await core.send("get_viewport_state");

      const after = core.latestDocument();
      const viewport = core.latestViewport();
      expect(featureKinds(after!)).toContain("extrude");
      expectExtrudeDepth(after!, 20);
      expectBodyCount(viewport!, 1);
      expectRevisionAtLeast(after!, 3);
    } finally {
      await core.close();
    }
  }, 60000);
});
