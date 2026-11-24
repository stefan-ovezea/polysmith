import {
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  sendCoreCommand,
} from "@/lib";
import type { CoreCommand } from "@/types";
import { useCadCoreStore } from "@/state";

// The first viewport build of an imported assembly compiles and tessellates
// every body (5.3 s on the 1,092-body carrier) and the core reports nothing
// while it runs, so the overlay would sit on the import's last line. It says
// what is happening instead.
async function noteViewportBuild(): Promise<void> {
  const { busyOperation, updateBusyOperation } = useCadCoreStore.getState();
  if (busyOperation) {
    updateBusyOperation("Building the viewport ...");
  }
}

export async function sendAndRefreshViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await noteViewportBuild();
  await sendCoreCommand(makeGetViewportStateCommand());
}

export async function sendAndRefreshSessionViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetSessionStateCommand());
  await noteViewportBuild();
  await sendCoreCommand(makeGetViewportStateCommand());
}

// Phase E2: refresh the session (undo/redo flags) but SKIP the ~2 MB viewport
// snapshot.  Used by mutations that do not change the body's triangles
// (appearance, rename) — the scene re-derives color/label from the document,
// so no viewport round-trip is needed.
export async function sendAndRefreshSession(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetSessionStateCommand());
}
