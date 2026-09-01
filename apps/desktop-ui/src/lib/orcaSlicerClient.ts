import { invoke } from "@tauri-apps/api/core";

import type { SlicerExportFormat } from "@/types";

export interface SlicerViewportBounds {
  x: number;
  y: number;
  width: number;
  height: number;
  scaleFactor: number;
}

export interface OrcaEmbedRequest {
  binaryPath: string;
  modelFilePath?: string | null;
  bounds: SlicerViewportBounds;
}

export interface OrcaLaunchRequest {
  binaryPath: string;
  modelFilePath?: string | null;
}

export interface OrcaEmbedResult {
  platform: string;
  processId: number;
  status: "embedded" | "running" | "hidden" | "unsupported";
  message: string;
}

export function prepareOrcaExportPath(
  format: SlicerExportFormat,
): Promise<string> {
  return invoke("prepare_orca_export_path", { format });
}

export function launchOrcaSlicer(
  request: OrcaLaunchRequest,
): Promise<OrcaEmbedResult> {
  return invoke("launch_orca_slicer", { request });
}

export function embedOrcaWindow(
  request: OrcaEmbedRequest,
): Promise<OrcaEmbedResult> {
  return invoke("embed_orca_window", { request });
}

export function resizeOrcaWindow(
  bounds: SlicerViewportBounds,
): Promise<OrcaEmbedResult> {
  return invoke("resize_orca_window", { bounds });
}

export function hideOrcaWindow(): Promise<OrcaEmbedResult> {
  return invoke("hide_orca_window");
}

export function setOrcaMapped(mapped: boolean): Promise<OrcaEmbedResult> {
  return invoke("set_orca_mapped", { mapped });
}
