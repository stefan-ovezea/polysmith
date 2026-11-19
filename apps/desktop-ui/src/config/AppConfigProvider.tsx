import {
  createContext,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import {
  applyTheme,
  bootstrapAppConfig,
  defaultAvailableThemes,
  defaultThemes,
  getActiveTheme,
  loadAppConfig,
  saveAppConfig,
} from "./appConfig";
import type { AppConfig, ThemeConfig } from "./types";

interface AppConfigContextValue {
  config: AppConfig;
  activeTheme: ThemeConfig;
  availableThemes: Array<Pick<ThemeConfig, "id" | "name">>;
  configPath: string | null;
  themesPath: string | null;
  isConfigReady: boolean;
  setConfig: (config: AppConfig) => void;
  updateConfig: (updater: (config: AppConfig) => AppConfig) => void;
}

const AppConfigContext = createContext<AppConfigContextValue | null>(null);

function compareThemeOptions(
  left: Pick<ThemeConfig, "id" | "name">,
  right: Pick<ThemeConfig, "id" | "name">,
) {
  return (
    left.name.localeCompare(right.name, undefined, { sensitivity: "base" }) ||
    left.id.localeCompare(right.id)
  );
}

export function AppConfigProvider({ children }: { children: ReactNode }) {
  const [config, setConfigState] = useState<AppConfig>(() => loadAppConfig());
  const [themes, setThemes] =
    useState<Record<string, ThemeConfig>>(() => defaultThemes);
  const [configPath, setConfigPath] = useState<string | null>(null);
  const [themesPath, setThemesPath] = useState<string | null>(null);
  const [isConfigReady, setIsConfigReady] = useState(false);
  const [systemThemeVersion, setSystemThemeVersion] = useState(0);
  const activeTheme = useMemo(
    () => getActiveTheme(config, themes),
    // `systemThemeVersion` is a deliberate invalidation signal: it carries no
    // value this memo reads, it exists so a change of the OS light/dark theme
    // re-resolves the "system" theme.
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [config, themes, systemThemeVersion],
  );

  useEffect(() => {
    let isMounted = true;

    void bootstrapAppConfig().then((bootstrap) => {
      if (!isMounted) {
        return;
      }

      setThemes(bootstrap.themes);
      setConfigState(bootstrap.config);
      setConfigPath(bootstrap.configPath);
      setThemesPath(bootstrap.themesPath);
      setIsConfigReady(true);
    });

    return () => {
      isMounted = false;
    };
  }, []);

  useEffect(() => {
    applyTheme(activeTheme);
  }, [activeTheme]);

  useEffect(() => {
    if (!isConfigReady) {
      return;
    }
    void saveAppConfig(config);
  }, [config, isConfigReady]);

  useEffect(() => {
    if (typeof window === "undefined") {
      return undefined;
    }

    const mediaQuery = window.matchMedia("(prefers-color-scheme: light)");
    const handleChange = () => {
      setSystemThemeVersion((version) => version + 1);
    };

    mediaQuery.addEventListener("change", handleChange);
    return () => {
      mediaQuery.removeEventListener("change", handleChange);
    };
  }, []);

  const value = useMemo<AppConfigContextValue>(
    () => ({
      config,
      activeTheme,
      availableThemes: [
        defaultAvailableThemes[0],
        ...Object.values(themes)
          .map((theme) => ({
            id: theme.id,
            name: theme.name,
          }))
          .sort(compareThemeOptions),
      ],
      configPath,
      themesPath,
      isConfigReady,
      setConfig: setConfigState,
      updateConfig: (updater) => {
        setConfigState((current) => updater(current));
      },
    }),
    [activeTheme, config, configPath, isConfigReady, themes, themesPath],
  );

  return (
    <AppConfigContext.Provider value={value}>
      {children}
    </AppConfigContext.Provider>
  );
}

export function useAppConfig() {
  const context = useContext(AppConfigContext);
  if (!context) {
    throw new Error("useAppConfig must be used inside AppConfigProvider");
  }
  return context;
}
