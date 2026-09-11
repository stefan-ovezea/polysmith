import { invoke } from "@tauri-apps/api/core";

// Thin invoke wrappers over the shell-side GRBL transport (see
// src-tauri/src/gcode_sender.rs).  State travels back to the UI as
// `grbl-stream` events, consumed by state/grblStore.ts.

export interface GrblPortInfo {
  name: string;
  portType: string;
}

export type GrblStreamEventKind =
  | "connected"
  | "disconnected"
  | "progress"
  | "status"
  | "error"
  | "completed"
  | "paused"
  | "resumed"
  | "reset";

export interface GrblStreamEvent {
  kind: GrblStreamEventKind;
  message: string;
  portName?: string | null;
  baudRate?: number | null;
  linesSent?: number | null;
  linesTotal?: number | null;
  percent?: number | null;
  state?: string | null;
  mpos?: [number, number, number] | null;
}

export function listGrblPorts(): Promise<GrblPortInfo[]> {
  return invoke("grbl_list_ports");
}

export function grblConnect(port: string, baudRate: number): Promise<void> {
  return invoke("grbl_connect", { port, baudRate });
}

export function grblDisconnect(): Promise<void> {
  return invoke("grbl_disconnect");
}

export function grblSendFile(filePath: string): Promise<void> {
  return invoke("grbl_send_file", { filePath });
}

export function grblPause(): Promise<void> {
  return invoke("grbl_pause");
}

export function grblResume(): Promise<void> {
  return invoke("grbl_resume");
}

export function grblReset(): Promise<void> {
  return invoke("grbl_reset");
}

export function grblHome(): Promise<void> {
  return invoke("grbl_home");
}

export function grblUnlock(): Promise<void> {
  return invoke("grbl_unlock");
}

export function grblJog(
  x: number | null,
  y: number | null,
  feed: number,
): Promise<void> {
  return invoke("grbl_jog", { x, y, feed });
}
