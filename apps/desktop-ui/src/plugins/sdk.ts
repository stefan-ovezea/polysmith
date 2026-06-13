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
  | "cylinder"
  | "profile_extrude"
  | "rounded_rect_profile_sweep";

export interface PluginProfilePoint {
  u: number;
  v: number;
}

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
  top_offset_x?: number;
  top_offset_y?: number;
  profile_plane?: "xy" | "xz" | "yz";
  extrude_x?: number;
  extrude_y?: number;
  extrude_z?: number;
  path_width?: number;
  path_depth?: number;
  path_radius?: number;
  profile_points?: PluginProfilePoint[];
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

export type PluginFeatureEntry = DocumentState["feature_history"][number];

export interface PluginCreatedFeatureResult {
  document: DocumentState;
  feature: PluginFeatureEntry;
  featureId: string;
  createdFeatures: PluginFeatureEntry[];
}

export interface PluginActionResult<TState = unknown> {
  actionId: string;
  featureId: string;
  state: TState;
}

export interface PluginActiveAction<TState = unknown>
  extends PluginActionResult<TState> {
  pluginId: string;
}

export interface PluginActionPanelProps<TState = unknown> {
  disabled: boolean;
  action: PluginActiveAction<TState>;
  onClose: () => void;
}

export interface PluginContext<TConfig> {
  manifest: PluginManifest;
  config: TConfig;
  document: DocumentState | null;
  viewport: ViewportState | null;
  sendCommand: (command: CoreCommand) => Promise<void>;
  refreshViewport: () => Promise<void>;
  awaitCreatedFeature: (
    predicate: (feature: PluginFeatureEntry) => boolean,
  ) => Promise<PluginCreatedFeatureResult>;
}

export interface PluginRuntime<TConfig> {
  menuItems: PluginMenuItem[];
  renderSettings: (props: PluginSettingsPanelProps<TConfig>) => ReactNode;
  handleCommand?: (command: string) => Promise<PluginActionResult | null | void>;
  renderAction?: (props: PluginActionPanelProps) => ReactNode;
}

export interface TrustedPlugin<TConfig = unknown> {
  manifest: PluginManifest;
  defaultConfig: TConfig;
  migrateConfig?: (config: TConfig) => TConfig;
  activate: (context: PluginContext<TConfig>) => PluginRuntime<TConfig>;
}

export function definePlugin<TConfig>(
  plugin: TrustedPlugin<TConfig>,
): TrustedPlugin<TConfig> {
  return plugin;
}
