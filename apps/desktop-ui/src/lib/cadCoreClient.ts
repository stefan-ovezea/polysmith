import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { CoreCommand, CoreMessage } from "../types/ipc";

export async function startCadCore(): Promise<string> {
  return invoke("start_cad_core");
}

export async function sendCoreCommand(command: CoreCommand): Promise<void> {
  return invoke("send_core_command", {
    command: JSON.stringify(command),
  });
}

export async function onCadCoreEvent(
  handler: (message: CoreMessage | Record<string, unknown>) => void,
): Promise<UnlistenFn> {
  return listen<CoreMessage | Record<string, unknown>>("cad-core-event", (event) => {
    handler(event.payload);
  });
}

export async function onCadCoreLog(
  handler: (line: string) => void,
): Promise<UnlistenFn> {
  return listen<string>("cad-core-log", (event) => {
    handler(event.payload);
  });
}

export async function onCadCoreError(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>("cad-core-error", (event) => {
    handler(event.payload);
  });
}

export async function onCadCoreExited(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>("cad-core-exited", (event) => {
    handler(event.payload);
  });
}
