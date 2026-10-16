import type { CamSetup, DocumentState } from "@/types";
import { awaitDocumentChange } from "@/state";
import { createDefaultCamSetup } from "@/layout/CamSetupPanel";

// A laser operation cannot join a mill setup — the core rejects it — and the
// PCB workspace's "Laser cut" button should not send the user off to the CAM
// workspace to configure a machine first.  So the button asks for a laser setup
// and gets one: the active setup when it is already a laser, otherwise an
// existing laser setup, otherwise a freshly created one.

interface EnsureLaserSetupContext {
  document: DocumentState | null;
  activeSetupId: string | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camSetupCreate: (setup: CamSetup) => Promise<void>;
  setActiveSetupId: (setupId: string) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

/// Name that does not collide after deletions and loads (the CAM workspace's
/// own setup-creation flow does the same).
function uniqueSetupName(existing: CamSetup[]): string {
  const taken = new Set(existing.map((setup) => setup.name));
  let index = existing.length + 1;
  while (taken.has(`Laser setup ${index}`)) {
    index += 1;
  }
  return `Laser setup ${index}`;
}

/**
 * Returns the setup id a PCB laser cut should join, creating a laser setup when
 * the project has none.  Returns null only when creation itself failed — the
 * caller then aborts rather than letting the core reject the operation.
 */
export async function ensureLaserSetup({
  document,
  activeSetupId,
  runAction,
  camSetupCreate,
  setActiveSetupId,
  addMessage,
  translate,
}: EnsureLaserSetupContext): Promise<string | null> {
  const setups = document?.cam.setups ?? [];

  // 1. The active setup, when it already cuts with a laser.
  const active = setups.find((setup) => setup.setup_id === activeSetupId);
  if (active && active.machine_type === "laser") {
    return active.setup_id;
  }

  // 2. Any laser setup in the project (and make it active, so CAM opens on
  //    the context the new operation belongs to).
  const existing = setups.find((setup) => setup.machine_type === "laser");
  if (existing) {
    setActiveSetupId(existing.setup_id);
    addMessage(translate("pcb.usingLaserSetup", { name: existing.name }));
    return existing.setup_id;
  }

  // 3. Create one.  The core auto-creates the laser TOOL for the operation, so
  //    a setup is all that is missing.
  const name = uniqueSetupName(setups);
  await runAction(async () => {
    await camSetupCreate({
      ...createDefaultCamSetup(),
      name,
      machine_type: "laser",
    });
  });
  const updated = await awaitDocumentChange(
    (next) => next.cam.setups.length > setups.length,
  );
  const created = updated.cam.setups[updated.cam.setups.length - 1];
  if (!created) {
    addMessage(translate("pcb.laserSetupFailed"));
    return null;
  }
  setActiveSetupId(created.setup_id);
  addMessage(translate("pcb.createdLaserSetup", { name: created.name }));
  return created.setup_id;
}
