// @vitest-environment node
import { afterEach, describe, expect, it, vi } from "vitest";

import {
  autoConstrainOfferDofThreshold,
  dismissAutoConstrainOffer,
  loadDismissedAutoConstrainOffers,
  shouldOfferAutoConstrain,
} from "./autoConstrainOffer";

function stubStorage(initial: Record<string, string> = {}) {
  const data = new Map(Object.entries(initial));
  const storage = {
    getItem: (key: string) => data.get(key) ?? null,
    setItem: (key: string, value: string) => {
      data.set(key, value);
    },
  };
  vi.stubGlobal("window", { localStorage: storage });
  return storage;
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("auto-constrain offer", () => {
  it("only asks when the sketch is genuinely loose", () => {
    const dismissed = new Set<string>();
    expect(
      shouldOfferAutoConstrain({ featureId: "feature-2", dofs: 21, dismissed }),
    ).toBe(true);
    expect(
      shouldOfferAutoConstrain({
        featureId: "feature-2",
        dofs: autoConstrainOfferDofThreshold,
        dismissed,
      }),
    ).toBe(false);
  });

  it("never asks when the solver has no number", () => {
    // -1 is "the solver did not run / could not measure". Unknown is not loose:
    // interrupting the user on a missing measurement would be a lie.
    const dismissed = new Set<string>();
    expect(
      shouldOfferAutoConstrain({ featureId: "feature-2", dofs: -1, dismissed }),
    ).toBe(false);
    expect(
      shouldOfferAutoConstrain({ featureId: "feature-2", dofs: null, dismissed }),
    ).toBe(false);
    expect(
      shouldOfferAutoConstrain({ featureId: null, dofs: 40, dismissed }),
    ).toBe(false);
  });

  it("stays quiet once the user silenced that sketch", () => {
    stubStorage();
    const afterDismiss = dismissAutoConstrainOffer("feature-2");
    expect(
      shouldOfferAutoConstrain({
        featureId: "feature-2",
        dofs: 40,
        dismissed: afterDismiss,
      }),
    ).toBe(false);
    // A different sketch is still offered.
    expect(
      shouldOfferAutoConstrain({
        featureId: "feature-3",
        dofs: 40,
        dismissed: afterDismiss,
      }),
    ).toBe(true);
    // And the silence survives a reload.
    expect(loadDismissedAutoConstrainOffers().has("feature-2")).toBe(true);
  });

  it("survives a missing or corrupted store", () => {
    vi.stubGlobal("window", {});
    expect(loadDismissedAutoConstrainOffers().size).toBe(0);
    stubStorage({ "polysmith.autoConstrainOfferDismissed": "not json" });
    expect(loadDismissedAutoConstrainOffers().size).toBe(0);
  });
});
