import type { CamOperation } from "@/types";
import { awaitDocumentChange } from "@/state/cadCoreStore";

// Shared helpers for the laser profile re-pick flow (App.tsx +
// CamFloatingPanels.tsx).

// Reports the ACTUAL post-command profile count.  The core may toggle
// several profiles for one entity click, so a client-side +1 guess is
// wrong in both directions.  Commands are fire-and-forget — the
// subscription is registered BEFORE firing so the reply event (which
// carries the new count) resolves it.
export async function reportCamProfileSelectionChange({
  beforeCount,
  runSelection,
  addMessage,
  translate,
}: {
  beforeCount: number;
  runSelection: () => Promise<void>;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}): Promise<void> {
  const updatedPromise = awaitDocumentChange(
    (next, previous) =>
      (next.selected_sketch_profile_ids?.length ?? 0) !==
      (previous?.selected_sketch_profile_ids?.length ?? 0),
  );
  await runSelection();
  const updated = await updatedPromise;
  const afterCount = updated.selected_sketch_profile_ids?.length ?? 0;
  if (afterCount > beforeCount) {
    addMessage(translate("cam.laserCut.profilePicked", { count: afterCount }));
  } else if (afterCount < beforeCount) {
    addMessage(translate("cam.laserCut.profileRemoved", { count: afterCount }));
  }
  // Net-zero (toggle off + on, or re-selecting the same profile set) —
  // deliberately no message.
}

// The reference sketch a laser operation cuts — every machining region
// must attest the same sketch, otherwise there is no single scope.
export function laserOperationScopeSketchId(
  operation: CamOperation | undefined,
): string | null {
  const regions = operation?.geometry_references.machining_regions ?? [];
  if (regions.length === 0) {
    return null;
  }
  const firstAttestation = regions[0]?.attestation;
  if (!firstAttestation || !("sketch_feature_id" in firstAttestation)) {
    return null;
  }
  const candidate = firstAttestation.sketch_feature_id;
  const allSame = regions.every(
    (region) =>
      "sketch_feature_id" in region.attestation &&
      region.attestation.sketch_feature_id === candidate,
  );
  return allSame ? candidate : null;
}
