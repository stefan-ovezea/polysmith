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

// The Rust bridge gzip-compresses events larger than 64KB (see
// protocol.rs emit_core_event). Decompress before handing the message
// to the handler; events are processed strictly in arrival order so
// document/viewport state updates never reorder.
async function decodeCoreEventPayload(
  payload: unknown,
): Promise<CoreMessage | Record<string, unknown>> {
  const wrapper = payload as { _gz?: string } | null;
  if (
    wrapper !== null &&
    typeof wrapper === "object" &&
    typeof wrapper._gz === "string"
  ) {
    const bytes = Uint8Array.from(atob(wrapper._gz), (char) =>
      char.charCodeAt(0),
    );
    const stream = new Blob([bytes])
      .stream()
      .pipeThrough(new DecompressionStream("gzip"));
    const text = await new Response(stream).text();
    return JSON.parse(text) as CoreMessage;
  }
  return payload as CoreMessage | Record<string, unknown>;
}

let eventChain: Promise<void> = Promise.resolve();

export async function onCadCoreEvent(
  handler: (message: CoreMessage | Record<string, unknown>) => void,
): Promise<UnlistenFn> {
  return listen<unknown>(CadCoreCommandType.CadCoreEvent, (event) => {
    eventChain = eventChain
      .then(() => decodeCoreEventPayload(event.payload))
      .then((message) => {
        handler(message);
      })
      .catch((error) => {
        console.error("failed to decode cad-core-event payload:", error);
      });
  });
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
