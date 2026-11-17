import { describe, expect, it } from "vitest";

import type { DocumentState } from "../types/ipc";
import { pendingExtrudeActionFromSelection } from "./extrudeCreationState";

const emptyDocument = {
  feature_history: [],
} as unknown as DocumentState;

describe("pendingExtrudeActionFromSelection", () => {
  // The panel renders the operation dropdown from `initialMode` (see
  // ExtrudePreviewPanel -> advanced.operation). `automaticMode: true` made the
  // create path send `mode: null`, so the dropdown said "New body" while the
  // core decided join/cut per profile group: 56 selected faces produced 20+
  // extrude blocks and no new bodies (the prisms were cut into the existing
  // plate). The action must carry the mode it displays.
  it("sends the mode the panel displays instead of asking the core to decide", () => {
    const action = pendingExtrudeActionFromSelection({
      document: emptyDocument,
      selectedSketchProfileIds: ["profile-circle-circle-1"],
      selectedExtrudableFaceId: null,
      selectedSketchEntityIds: [],
      defaultSettings: { mode: "new_body", targetBodyId: null },
    });

    expect(action.automaticMode).toBe(false);
    expect(action.initialMode).toBe("new_body");
    expect(action.profileIds).toEqual(["profile-circle-circle-1"]);
  });

  it("keeps the resolved join target for a sketch on a body face", () => {
    const action = pendingExtrudeActionFromSelection({
      document: emptyDocument,
      selectedSketchProfileIds: ["profile-circle-circle-1"],
      selectedExtrudableFaceId: null,
      selectedSketchEntityIds: [],
      defaultSettings: { mode: "join", targetBodyId: "feature-7" },
    });

    expect(action.automaticMode).toBe(false);
    expect(action.initialMode).toBe("join");
    expect(action.initialTargetBodyId).toBe("feature-7");
  });
});
