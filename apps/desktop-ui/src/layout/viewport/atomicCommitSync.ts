// The one decision that retires the commit-flash hold.
//
// A drag commit mutates the scene objects locally so the preview never snaps
// back, then sends the core a command. If that reply carried the viewport with
// it (`include_viewport`), the scene is already current and the sync must
// rebuild straight away. If it did not, the viewport describing the commit has
// not arrived yet and the mutated preview must be held until it does —
// otherwise the pre-commit position is painted for a frame.
//
// Kept dependency-free so it can be unit-tested: `syncViewportScene` takes
// ~40 parameters and is not worth a harness for one comparison.

/**
 * True when the viewport already describes the document that just landed.
 *
 * Both numbers are revisions: `ViewportState.revision` is the document's
 * `geometry_revision` at the moment the viewport was built. A viewport built
 * after the document is therefore current; one built before it is stale and the
 * preview has to be held.
 *
 * @param viewportRevision revision stamped on the viewport payload (-1 when absent)
 * @param documentRevision revision of the document now in the store
 */
export function viewportDescribesCommit({
  viewportRevision,
  documentRevision,
}: {
  viewportRevision?: number | null;
  documentRevision?: number | null;
}): boolean {
  const viewport = viewportRevision ?? -1;
  const document = documentRevision ?? -1;
  // Payloads predating the field carry no revision, so `viewport < 0` means
  // "unknown" and the caller must fall back to the hash hold.
  return viewport >= 0 && viewport >= document;
}
