import { describe, expect, it, vi } from "vitest";

import type { ActiveExtrudeAction } from "./appState";
import type { CancelActiveToolContext } from "./toolCancellation";
import { cancelActiveToolFromContext } from "./toolCancellation";

// Reported: "cancel does not work because is not restoring the original
// selection and the refresh creates a body from all selected surfaces during
// edit". The edit snapshot carried the extent and the operation but NOT the
// surfaces, so the restore re-issued the old depth onto the selection the edit
// had left behind — the body stayed as the edit made it, i.e. Cancel acted as
// Confirm.
function fakeContext(
  extrudeAction: ActiveExtrudeAction,
  calls: string[],
): CancelActiveToolContext {
  const record =
    (name: string, result: unknown = undefined) =>
    async (...args: unknown[]) => {
      calls.push(`${name}:${args.slice(1).map(String).join(",")}`);
      return result as never;
    };
  return {
    actions: {
      extrudeAction,
      loftAction: null,
      revolveAction: null,
      sweepAction: null,
      moveAction: null,
      edgeOpAction: null,
      shellAction: null,
      holeAction: null,
      offsetPlaneAction: null,
      anglePlaneAction: null,
      midplaneAction: null,
      tangentPlaneAction: null,
      constructionAxisAction: null,
      constructionPointAction: null,
      threadAction: null,
      fastenerAction: null,
      helixAction: null,
      pluginAction: null,
      editingFeatureId: null,
      materialsPanelOpen: false,
      sketchTextAction: null,
      drawingToolArmed: false,
    } as unknown as CancelActiveToolContext["actions"],
    setters: {
      setExtrudeAction: (value: unknown) => {
        calls.push(`setExtrudeAction:${value === null ? "null" : "value"}`);
      },
    } as unknown as CancelActiveToolContext["setters"],
    activeEdgeIdsRef: { current: [] },
    runAction: async (action) => {
      calls.push("runAction");
      await action();
    },
    restoreTimelineCursorAfterEdit: async () => {
      calls.push("restoreCursor");
    },
    undo: async () => {
      calls.push("undo");
    },
    setSketchTool: async () => {},
    deleteSketchText: async () => {},
    undoUntilExtrudePreviewRemoved: async () => {
      calls.push("undoUntilRemoved");
    },
    updateLoftProfiles: async () => {},
    updateLoftRuled: async () => {},
    updateRevolveProfile: async () => {},
    updateRevolveAxis: async () => {},
    updateRevolveAngle: async () => {},
    updateSweepProfile: async () => {},
    updateSweepPath: async () => {},
    updateMoveParameters: async () => {},
    updateThreadParameters: async () => {},
    updateFastenerParameters: async () => {},
    deleteFeature: async () => {},
    updateExtrudeDepth: record("updateDepth"),
    updateExtrudeMode: record("updateMode"),
    updateExtrudeTargetBody: record("updateTarget"),
    updateExtrudeParameters: record("updateParameters"),
    updateExtrudeProfiles: record("updateProfiles"),
  };
}

function editSession(profileIds: string[]): ActiveExtrudeAction {
  return {
    phase: "active",
    featureId: "feature-7",
    featureIds: ["feature-7"],
    profileIds: [...profileIds],
    automaticMode: false,
    initialDepth: 20,
    initialMode: "new_body",
    initialParameters: null,
    initialTargetBodyId: null,
    profileCount: profileIds.length,
    canCombineWithExistingBody: false,
    originalSnapshot: {
      depth: 20,
      mode: "new_body",
      targetBodyId: null,
      parameters: {} as never,
      profileIds: ["profile-circle-circle-1"],
    },
  };
}

describe("cancel of an extrude edit", () => {
  it("restores the surfaces the edit started with, before the extent", async () => {
    const calls: string[] = [];
    await cancelActiveToolFromContext(
      fakeContext(editSession(["profile-circle-circle-1", "profile-circle-circle-2"]), calls),
    );

    const profilesAt = calls.findIndex((call) => call.startsWith("updateProfiles"));
    const depthAt = calls.findIndex((call) => call.startsWith("updateDepth"));
    expect(profilesAt).toBeGreaterThanOrEqual(0);
    expect(depthAt).toBeGreaterThan(profilesAt);
    expect(calls[profilesAt]).toContain("profile-circle-circle-1");
    expect(calls[profilesAt]).not.toContain("circle-2");
  });

  it("closes the panel before restoring, so the selection sync cannot re-apply the edit", async () => {
    const calls: string[] = [];
    await cancelActiveToolFromContext(
      fakeContext(editSession(["profile-circle-circle-1"]), calls),
    );

    const closedAt = calls.indexOf("setExtrudeAction:null");
    const restoreAt = calls.findIndex((call) => call.startsWith("updateProfiles"));
    expect(closedAt).toBeGreaterThanOrEqual(0);
    expect(restoreAt).toBeGreaterThan(closedAt);
  });
});
