import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import type { DocumentState, ViewportState } from "@/types";
import { bundledPlugins } from "./bundled";
import {
  bootstrapPluginConfig,
  savePluginConfig,
  type PluginConfigDocument,
} from "./pluginConfig";
import type { PluginMenuItem, PluginRuntime, TrustedPlugin } from "./sdk";

interface PluginRuntimeEntry<TConfig = unknown> {
  plugin: TrustedPlugin<TConfig>;
  enabled: boolean;
  config: TConfig;
  runtime: PluginRuntime<TConfig>;
}

interface PluginHostContextValue {
  configPath: string | null;
  isPluginConfigReady: boolean;
  plugins: PluginRuntimeEntry[];
  menuItems: Array<PluginMenuItem & { pluginId: string }>;
  setPluginEnabled: (pluginId: string, enabled: boolean) => void;
  updatePluginConfig: <TConfig,>(
    pluginId: string,
    updater: (config: TConfig) => TConfig,
  ) => void;
}

const PluginHostContext = createContext<PluginHostContextValue | null>(null);

function initialPluginConfig(): PluginConfigDocument {
  return {
    plugins: Object.fromEntries(
      bundledPlugins.map((plugin) => [
        plugin.manifest.id,
        {
          enabled: true,
          config: plugin.defaultConfig,
        },
      ]),
    ),
  };
}

export function PluginProvider({
  children,
  document,
  viewport,
}: {
  children: ReactNode;
  document: DocumentState | null;
  viewport: ViewportState | null;
}) {
  const [pluginConfig, setPluginConfig] =
    useState<PluginConfigDocument>(initialPluginConfig);
  const [configPath, setConfigPath] = useState<string | null>(null);
  const [isPluginConfigReady, setIsPluginConfigReady] = useState(false);

  useEffect(() => {
    let isMounted = true;
    void bootstrapPluginConfig(bundledPlugins).then((bootstrap) => {
      if (!isMounted) {
        return;
      }
      setPluginConfig(bootstrap.config);
      setConfigPath(bootstrap.configPath);
      setIsPluginConfigReady(true);
    });
    return () => {
      isMounted = false;
    };
  }, []);

  useEffect(() => {
    if (!isPluginConfigReady) {
      return;
    }
    void savePluginConfig(pluginConfig);
  }, [isPluginConfigReady, pluginConfig]);

  const plugins = useMemo<PluginRuntimeEntry[]>(() => {
    return bundledPlugins.map((plugin) => {
      const entry = pluginConfig.plugins[plugin.manifest.id] ?? {
        enabled: true,
        config: plugin.defaultConfig,
      };
      const runtime = plugin.activate({
        manifest: plugin.manifest,
        config: entry.config,
        document,
        viewport,
        sendCommand: sendCoreCommand,
        refreshViewport: async () => {
          await sendCoreCommand(makeGetViewportStateCommand());
        },
      });
      return {
        plugin,
        enabled: entry.enabled,
        config: entry.config,
        runtime,
      };
    });
  }, [document, pluginConfig.plugins, viewport]);

  const menuItems = useMemo(
    () =>
      plugins.flatMap((entry) =>
        entry.enabled
          ? entry.runtime.menuItems.map((item) => ({
              ...item,
              pluginId: entry.plugin.manifest.id,
            }))
          : [],
      ),
    [plugins],
  );

  const value = useMemo<PluginHostContextValue>(
    () => ({
      configPath,
      isPluginConfigReady,
      plugins,
      menuItems,
      setPluginEnabled: (pluginId, enabled) => {
        setPluginConfig((current) => ({
          plugins: {
            ...current.plugins,
            [pluginId]: {
              enabled,
              config:
                current.plugins[pluginId]?.config ??
                bundledPlugins.find((plugin) => plugin.manifest.id === pluginId)
                  ?.defaultConfig ??
                {},
            },
          },
        }));
      },
      updatePluginConfig: (pluginId, updater) => {
        setPluginConfig((current) => ({
          plugins: {
            ...current.plugins,
            [pluginId]: {
              enabled: current.plugins[pluginId]?.enabled ?? true,
              config: updater(
                (current.plugins[pluginId]?.config ??
                  bundledPlugins.find((plugin) => plugin.manifest.id === pluginId)
                    ?.defaultConfig ??
                  {}) as never,
              ),
            },
          },
        }));
      },
    }),
    [configPath, isPluginConfigReady, menuItems, plugins],
  );

  return (
    <PluginHostContext.Provider value={value}>
      {children}
    </PluginHostContext.Provider>
  );
}

export function usePluginHost() {
  const context = useContext(PluginHostContext);
  if (!context) {
    throw new Error("usePluginHost must be used inside PluginProvider");
  }
  return context;
}
