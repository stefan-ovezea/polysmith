// The timeline "show everything" sentinel must survive the trip to the core.
//
// The core reads `included_action_count` with `json::get<int>()` and clamps it as
// `std::max(0, std::min(count, actions))`. `Number.MAX_SAFE_INTEGER` truncates to
// -1 on that read, which clamps to 0 — so every operation ended with the marker
// at the START of the timeline and every body hidden. Reported as "after each
// operation the cursor goes to 0 or beginning. I drag it and all is ok."
import { describe, expect, it } from "vitest";

import { TIMELINE_END_ACTION_COUNT } from "./timelineEditSession";

const INT_MAX = 2_147_483_647;

describe("timeline cursor end sentinel", () => {
  it("fits in the int the core parses it as", () => {
    expect(Number.isInteger(TIMELINE_END_ACTION_COUNT)).toBe(true);
    expect(TIMELINE_END_ACTION_COUNT).toBeGreaterThan(0);
    expect(TIMELINE_END_ACTION_COUNT).toBeLessThanOrEqual(INT_MAX);
  });

  it("is far past any plausible action count, so the core clamps it to all", () => {
    expect(TIMELINE_END_ACTION_COUNT).toBeGreaterThan(10_000);
  });
});
