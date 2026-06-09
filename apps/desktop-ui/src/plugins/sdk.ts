import type { ReactNode } from "react";
import type { CoreCommand, DocumentState, ViewportState } from "@/types";

export interface PluginManifest {
  id: string;
  name: string;
  version: string;
  sdkVersion: string;
  description: string;
}

export type PluginGeometryOperationKind = "add" | "subtract";
export type PluginGeometryPrimitive =
  | "box"
  | "rounded_box"
  | "tapered_rounded_box"
  | "cylinder";

export interface PluginGeometryOperation {
  operation: PluginGeometryOperationKind;
  primitive: PluginGeometryPrimitive;
  x: number;
  y: number;
  z: number;
  width: number;
  depth: number;
  height: number;
  radius?: number;
  top_width?: number;
  top_depth?: number;
  top_radius?: number;
}

export interface PluginMenuItem {
  id: string;
  labelKey: string;
  command: string;
  disabledWhenCoreOffline?: boolean;
}

export interface PluginSettingsPanelProps<TConfig> {
  config: TConfig;
  disabled: boolean;
  onChange: (config: TConfig) => void;
}

export interface PluginContext<TConfig> {
  manifest: PluginManifest;
  config: TConfig;
  document: DocumentState | null;
  viewport: ViewportState | null;
  sendCommand: (command: CoreCommand) => Promise<void>;
  refreshViewport: () => Promise<void>;
}

export interface PluginRuntime<TConfig> {
  menuItems: PluginMenuItem[];
  renderSettings: (props: PluginSettingsPanelProps<TConfig>) => ReactNode;
}

export interface TrustedPlugin<TConfig = unknown> {
  manifest: PluginManifest;
  defaultConfig: TConfig;
  activate: (context: PluginContext<TConfig>) => PluginRuntime<TConfig>;
}

export function definePlugin<TConfig>(
  plugin: TrustedPlugin<TConfig>,
): TrustedPlugin<TConfig> {
  return plugin;
}
