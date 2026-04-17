import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { CoreCommand, CoreMessage } from "@/types";

enum CadCoreCommandType {
  SendCoreCommand = "send_core_command",
  StartCadCore = "start_cad_core",
  CadCoreEvent = "cad-core-event",
  CadCoreLog = "cad-core-log",
  CadCoreError = "cad-core-error",
  CadCoreExited = "cad-core-exited",
}

export async function startCadCore(): Promise<string> {
  return invoke(CadCoreCommandType.StartCadCore);
}

export async function sendCoreCommand(command: CoreCommand): Promise<void> {
  return invoke(CadCoreCommandType.SendCoreCommand, {
    command: JSON.stringify(command),
  });
}

export async function onCadCoreEvent(
  handler: (message: CoreMessage | Record<string, unknown>) => void,
): Promise<UnlistenFn> {
  return listen<CoreMessage | Record<string, unknown>>(
    CadCoreCommandType.CadCoreEvent,
    (event) => {
      handler(event.payload);
    },
  );
}

export async function onCadCoreLog(
  handler: (line: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreLog, (event) => {
    handler(event.payload);
  });
}

export async function onCadCoreError(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreError, (event) => {
    handler(event.payload);
  });
}

export async function onCadCoreExited(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreExited, (event) => {
    handler(event.payload);
  });
}
