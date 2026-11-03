import { describe, expect, it } from "vitest";

import { viewportDescribesCommit } from "./atomicCommitSync";

describe("viewportDescribesCommit", () => {
  it("is current when the viewport was built from this very document", () => {
    expect(
      viewportDescribesCommit({ viewportRevision: 31, documentRevision: 31 }),
    ).toBe(true);
  });

  it("is current when the viewport is ahead of the document", () => {
    expect(
      viewportDescribesCommit({ viewportRevision: 32, documentRevision: 31 }),
    ).toBe(true);
  });

  it("is stale when the viewport predates the document", () => {
    // The non-atomic reply case: the document moved, the viewport has not.
    expect(
      viewportDescribesCommit({ viewportRevision: 30, documentRevision: 31 }),
    ).toBe(false);
  });

  it("is stale when the payload carries no revision at all", () => {
    // Payloads from a core that predates the field: fall back to the hash hold.
    expect(viewportDescribesCommit({ documentRevision: 31 })).toBe(false);
    expect(
      viewportDescribesCommit({ viewportRevision: null, documentRevision: 31 }),
    ).toBe(false);
  });

  it("is current for revision 0 when there is no document yet", () => {
    expect(
      viewportDescribesCommit({ viewportRevision: 0, documentRevision: -1 }),
    ).toBe(true);
  });
});
