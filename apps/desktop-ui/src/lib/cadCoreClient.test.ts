import { describe, expect, it } from "vitest";

import { shouldFailPendingCommand } from "./cadCoreClient";

// The UI used to fail any awaited core command after a flat 10 s. The core
// handles commands strictly in arrival order, so a short command sent during a
// long one waited for it — importing this board's STEP takes ~12 s, and a
// sketch's undo_end_group queued behind it was answered 11.9 s after it was
// sent, i.e. 100 ms after the import finished. The user saw
// "core did not respond to undo_end_group" for a healthy core.
//
// The rule is now "the CORE went quiet", measured from the last message it sent,
// with an absolute cap for a core that keeps talking but never answers.

const IDLE = 10_000;
const CAP = 300_000;

describe("core command timeout", () => {
  it("keeps a command queued behind a long, chatty operation alive", () => {
    // Sent 12 s ago, but the core logged progress 200 ms ago: still working.
    expect(
      shouldFailPendingCommand({
        now: 1_000_000,
        sentAt: 1_000_000 - 12_000,
        lastActivityAt: 1_000_000 - 200,
      }),
    ).toBe(false);
  });

  it("fails when the core has been silent for the idle window", () => {
    expect(
      shouldFailPendingCommand({
        now: 1_000_000,
        sentAt: 1_000_000 - 11_000,
        lastActivityAt: 1_000_000 - IDLE,
      }),
    ).toBe(true);
  });

  it("gives a freshly sent command its own full idle window", () => {
    // The core went quiet before this command was sent; the command must not be
    // failed for the silence that preceded it.
    expect(
      shouldFailPendingCommand({
        now: 1_000_000,
        sentAt: 1_000_000 - 1_000,
        lastActivityAt: 1_000_000 - 60_000,
      }),
    ).toBe(false);
  });

  it("still fails a command the core never answers, however chatty it is", () => {
    expect(
      shouldFailPendingCommand({
        now: 1_000_000,
        sentAt: 1_000_000 - CAP,
        lastActivityAt: 1_000_000 - 1,
      }),
    ).toBe(true);
  });
});
