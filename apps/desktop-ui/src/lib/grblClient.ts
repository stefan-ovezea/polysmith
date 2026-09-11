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
  | "reset"
  | "zeroed";

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
  /** WCS position (MPos minus the G54 offset) — the job coordinates. */
  wpos?: [number, number, number] | null;
}

export function listGrblPorts(): Promise<GrblPortInfo[]> {
  return invoke("grbl_list_ports");
}

export function grblConnect(port: string, baudRate: number): Promise<void> {
  return invoke("grbl_connect", { port, baudRate });
}

/** FluidNC's TCP text port (default 23) — same protocol as USB serial. */
export function grblConnectTcp(host: string, port: number): Promise<void> {
  return invoke("grbl_connect_tcp", { host, port });
}

export function grblDisconnect(): Promise<void> {
  return invoke("grbl_disconnect");
}

export function grblSendFile(filePath: string): Promise<void> {
  return invoke("grbl_send_file", { filePath });
}

// In-memory program send (CAM→GRBL handoff) — same worker pipeline as
// grblSendFile, no file on disk.
export function grblSendProgram(text: string): Promise<void> {
  return invoke("grbl_send_program", { text });
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

// Sets the WCS origin to the machine's current position (`G92 X0 Y0`)
// — the "zero XY here" laser workflow step.
export function grblZeroXy(): Promise<void> {
  return invoke("grbl_zero_xy");
}

// Mini console — sends one raw line to the controller verbatim
// (e.g. "$20=0" to disable soft limits).
export function grblSendRaw(line: string): Promise<void> {
  return invoke("grbl_send_raw", { line });
}

// ── G-code parsing (shell-side gcode_parser.rs) ────────────────────
// `grbl_parse_file` turns an exported .nc into structured moves for the
// GRBL workspace preview. Field names mirror the Rust serde camelCase
// output; `move.line` maps 1:1 onto the sender's linesSent counter.

export type GcodeMoveKind = "rapid" | "feed" | "arcCw" | "arcCcw" | "dwell";

export interface GcodeMove {
  /** Sequential move index (0-based) across the filtered stream. */
  index: number;
  /** 1-based index in the filtered line stream (= linesSent). */
  line: number;
  kind: GcodeMoveKind;
  start: [number, number, number];
  end: [number, number, number];
  /** Arc center (G2/G3, I/J offsets from start). */
  center: [number, number, number] | null;
  radius: number | null;
  /** mm/min (G20 inches are scaled to mm). */
  feed: number | null;
  /** Last S value seen (raw controller units). */
  power: number | null;
  /** Effective laser state during the move. */
  laserOn: boolean;
  /** G4 dwell in seconds (pierce marker). */
  dwellSeconds: number | null;
}

export interface GcodeBounds {
  minX: number;
  minY: number;
  minZ: number;
  maxX: number;
  maxY: number;
  maxZ: number;
}

export interface GcodeFileInfo {
  fileName: string;
  moves: GcodeMove[];
  bounds: GcodeBounds | null;
  unitsMm: boolean;
  absolute: boolean;
  warnings: string[];
}

export function grblParseFile(filePath: string): Promise<GcodeFileInfo> {
  return invoke("grbl_parse_file", { filePath });
}

// In-memory parse for the CAM→GRBL handoff: `label` becomes fileName.
export function grblParseText(
  text: string,
  label: string,
): Promise<GcodeFileInfo> {
  return invoke("grbl_parse_text", { text, label });
}
