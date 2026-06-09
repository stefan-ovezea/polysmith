import { invoke } from "@tauri-apps/api/core";

import type { TrustedPlugin } from "./sdk";

const PLUGIN_CONFIG_STORAGE_KEY = "polysmith.pluginConfig";

export interface PluginConfigEntry<TConfig = unknown> {
  enabled: boolean;
  config: TConfig;
}

export interface PluginConfigDocument {
  plugins: Record<string, PluginConfigEntry>;
}

interface NativePluginConfigBootstrap {
  config_path: string;
  config: unknown;
}

interface NativeDefaultPluginConfig {
  plugin_id: string;
  enabled: boolean;
  config: unknown;
}

export interface PluginConfigBootstrap {
  config: PluginConfigDocument;
  configPath: string | null;
}

function cloneConfig<TConfig>(config: TConfig): TConfig {
  return JSON.parse(JSON.stringify(config)) as TConfig;
}

function migrateBundledPluginConfig(
  pluginId: string,
  config: Record<string, unknown>,
): Record<string, unknown> {
  if (
    pluginId === "polysmith.gridfinity" &&
    config.configSchemaVersion !== 3 &&
    config.gridX === 2 &&
    config.gridY === 2 &&
    config.compartmentsX === 2 &&
    config.compartmentsY === 2
  ) {
    return {
      ...config,
      configSchemaVersion: 3,
      compartmentsX: 1,
      compartmentsY: 1,
    };
  }
  if (pluginId === "polysmith.gridfinity") {
    return {
      ...config,
      configSchemaVersion: 3,
    };
  }
  return config;
}

function normalizePluginConfig(
  input: unknown,
  plugins: TrustedPlugin[],
): PluginConfigDocument {
  const inputPlugins =
    input &&
    typeof input === "object" &&
    "plugins" in input &&
    typeof (input as { plugins?: unknown }).plugins === "object" &&
    (input as { plugins?: unknown }).plugins !== null
      ? ((input as { plugins: Record<string, unknown> }).plugins)
      : {};

  const normalized: PluginConfigDocument = { plugins: {} };
  for (const plugin of plugins) {
    const raw = inputPlugins[plugin.manifest.id];
    const enabled =
      raw &&
      typeof raw === "object" &&
      typeof (raw as { enabled?: unknown }).enabled === "boolean"
        ? Boolean((raw as { enabled: boolean }).enabled)
        : true;
    const rawConfig =
      raw &&
      typeof raw === "object" &&
      "config" in raw &&
      (raw as { config?: unknown }).config &&
      typeof (raw as { config?: unknown }).config === "object"
        ? (raw as { config: unknown }).config
        : {};
    const config =
      plugin.defaultConfig && typeof plugin.defaultConfig === "object"
        ? migrateBundledPluginConfig(plugin.manifest.id, {
            ...(cloneConfig(plugin.defaultConfig) as Record<string, unknown>),
            ...(rawConfig as Record<string, unknown>),
          })
        : rawConfig;

    normalized.plugins[plugin.manifest.id] = {
      enabled,
      config,
    };
  }
  return normalized;
}

function defaultPluginEntries(plugins: TrustedPlugin[]): NativeDefaultPluginConfig[] {
  return plugins.map((plugin) => ({
    plugin_id: plugin.manifest.id,
    enabled: true,
    config: plugin.defaultConfig,
  }));
}

function loadLocalPluginConfig(plugins: TrustedPlugin[]): PluginConfigDocument {
  if (typeof window === "undefined") {
    return normalizePluginConfig({}, plugins);
  }
  const stored = window.localStorage.getItem(PLUGIN_CONFIG_STORAGE_KEY);
  if (!stored) {
    return normalizePluginConfig({}, plugins);
  }
  try {
    return normalizePluginConfig(JSON.parse(stored), plugins);
  } catch {
    return normalizePluginConfig({}, plugins);
  }
}

function saveLocalPluginConfig(config: PluginConfigDocument): void {
  if (typeof window === "undefined") {
    return;
  }
  window.localStorage.setItem(PLUGIN_CONFIG_STORAGE_KEY, JSON.stringify(config));
}

export async function bootstrapPluginConfig(
  plugins: TrustedPlugin[],
): Promise<PluginConfigBootstrap> {
  try {
    const bootstrap = await invoke<NativePluginConfigBootstrap>(
      "bootstrap_plugin_config",
      {
        defaultPlugins: defaultPluginEntries(plugins),
      },
    );
    return {
      config: normalizePluginConfig(bootstrap.config, plugins),
      configPath: bootstrap.config_path,
    };
  } catch {
    return {
      config: loadLocalPluginConfig(plugins),
      configPath: null,
    };
  }
}

export async function savePluginConfig(
  config: PluginConfigDocument,
): Promise<void> {
  try {
    await invoke("save_plugin_config", { config });
  } catch {
    saveLocalPluginConfig(config);
  }
}
