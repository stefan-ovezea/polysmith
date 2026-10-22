import {
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  sendCoreCommand,
} from "@/lib";
import type { CoreCommand } from "@/types";

export async function sendAndRefreshViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetViewportStateCommand());
}

export async function sendAndRefreshSessionViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetSessionStateCommand());
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
