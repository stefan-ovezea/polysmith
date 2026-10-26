// S11 follow-up: offer "Fully constrain" once, when a sketch is being finished
// while it is still far from determined.
//
// Deliberately NOT automatic. An offer that runs by itself changes a sketch the
// user did not ask to change; this only ever appears on the way OUT of a sketch,
// only when the sketch is genuinely loose, and can be silenced per sketch.
//
// Thresholds come from the completion plan: DOF > 20 is "the user drew something
// and left it floating", while a sketch at 3 DOF is nearly done and does not need
// to be nagged about.

export const autoConstrainOfferDofThreshold = 20;

const dismissedStorageKey = "polysmith.autoConstrainOfferDismissed";

function readStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    // Storage can throw in a restricted context (private mode, blocked cookies).
    return null;
  }
}

/** Sketch ids whose offer the user silenced ("don't ask again"). */
export function loadDismissedAutoConstrainOffers(): Set<string> {
  const storage = readStorage();
  if (storage === null) return new Set();
  try {
    const raw = storage.getItem(dismissedStorageKey);
    if (!raw) return new Set();
    const parsed: unknown = JSON.parse(raw);
    if (!Array.isArray(parsed)) return new Set();
    return new Set(parsed.filter((id): id is string => typeof id === "string"));
  } catch {
    return new Set();
  }
}

/** Records the silence and returns the new set, so callers can keep it in state. */
export function dismissAutoConstrainOffer(featureId: string): Set<string> {
  const dismissed = loadDismissedAutoConstrainOffers();
  dismissed.add(featureId);
  const storage = readStorage();
  if (storage !== null) {
    try {
      storage.setItem(dismissedStorageKey, JSON.stringify([...dismissed]));
    } catch {
      // A full/blocked storage must not break finishing the sketch.
    }
  }
  return dismissed;
}

/**
 * Whether finishing this sketch should ask first.
 *
 * `dofs < 0` means the solver has no number (it did not run or could not
 * measure), which is not a reason to interrupt: unknown is not loose.
 */
export function shouldOfferAutoConstrain({
  featureId,
  dofs,
  dismissed,
}: {
  featureId: string | null;
  dofs: number | null | undefined;
  dismissed: ReadonlySet<string>;
}): boolean {
  if (featureId === null || featureId.length === 0) return false;
  if (typeof dofs !== "number" || !Number.isFinite(dofs) || dofs < 0) {
    return false;
  }
  if (dismissed.has(featureId)) return false;
  return dofs > autoConstrainOfferDofThreshold;
}
