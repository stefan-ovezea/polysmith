import { invoke } from "@tauri-apps/api/core";
import defaultConfig from "./config.json";
import catppuccinFrappeTheme from "./themes/catppuccin-frappe.json";
import catppuccinLatteTheme from "./themes/catppuccin-latte.json";
import catppuccinMacchiatoTheme from "./themes/catppuccin-macchiato.json";
import catppuccinMochaTheme from "./themes/catppuccin-mocha.json";
import darkTheme from "./themes/dark.json";
import lightTheme from "./themes/light.json";
import type {
  AppConfig,
  CrosshairMode,
  HotkeyBinding,
  ThemeConfig,
  ThemeSelection,
} from "./types";

const bundledThemes: Record<string, ThemeConfig> = {
  dark: darkTheme,
  light: lightTheme,
  "catppuccin-latte": catppuccinLatteTheme,
  "catppuccin-frappe": catppuccinFrappeTheme,
  "catppuccin-macchiato": catppuccinMacchiatoTheme,
  "catppuccin-mocha": catppuccinMochaTheme,
};

const CONFIG_STORAGE_KEY = "polysmith.appConfig";

const DEFAULT_THEME_FILES = [
  { fileName: "dark.json", contents: darkTheme },
  { fileName: "light.json", contents: lightTheme },
  { fileName: "catppuccin-latte.json", contents: catppuccinLatteTheme },
  { fileName: "catppuccin-frappe.json", contents: catppuccinFrappeTheme },
  { fileName: "catppuccin-macchiato.json", contents: catppuccinMacchiatoTheme },
  { fileName: "catppuccin-mocha.json", contents: catppuccinMochaTheme },
];

export const defaultAppConfig = defaultConfig as AppConfig;
const SYSTEM_THEME_ID = "system";
const systemThemeOption = {
  id: SYSTEM_THEME_ID,
  name: "System",
} as const;
export const defaultThemes = bundledThemes;
export const defaultAvailableThemes = [
  systemThemeOption,
  ...Object.values(bundledThemes),
];

export interface AppConfigBootstrap {
  config: AppConfig;
  themes: Record<string, ThemeConfig>;
  configPath: string | null;
  themesPath: string | null;
}

interface NativeConfigBootstrap {
  config_path: string;
  themes_path: string;
  config: Partial<AppConfig>;
  themes: unknown[];
}

function getSystemThemeId(): "dark" | "light" {
  if (typeof window === "undefined") {
    return "dark";
  }

  return window.matchMedia("(prefers-color-scheme: light)").matches
    ? "light"
    : "dark";
}

function isThemeSelection(value: string): value is ThemeSelection {
  return value.length > 0;
}

export function isThemeAvailable(
  value: string,
  themes: Record<string, unknown>,
): value is ThemeSelection {
  return value === SYSTEM_THEME_ID || value in themes;
}

function isCrosshairMode(value: unknown): value is CrosshairMode {
  return (
    value === "default" ||
    value === "viewport-25" ||
    value === "viewport-50" ||
    value === "viewport-75" ||
    value === "infinite"
  );
}

function normalizeAiConfig(input: Partial<AppConfig>["ai"]): AppConfig["ai"] {
  const defaults = defaultAppConfig.ai;
  const provider = input?.provider === "ollama" ? input.provider : defaults.provider;
  const maxAgentSteps =
    typeof input?.maxAgentSteps === "number" &&
    Number.isFinite(input.maxAgentSteps) &&
    input.maxAgentSteps > 0
      ? Math.min(Math.max(Math.round(input.maxAgentSteps), 1), 10)
      : defaults.maxAgentSteps;

  return {
    enabled:
      typeof input?.enabled === "boolean" ? input.enabled : defaults.enabled,
    provider,
    baseUrl:
      typeof input?.baseUrl === "string" && input.baseUrl.trim().length > 0
        ? input.baseUrl
        : defaults.baseUrl,
    model: typeof input?.model === "string" ? input.model : defaults.model,
    previewBeforeRun:
      typeof input?.previewBeforeRun === "boolean"
        ? input.previewBeforeRun
        : defaults.previewBeforeRun,
    maxAgentSteps,
  };
}

function normalizeOrcaSlicerConfig(
  input: Partial<AppConfig>["orcaSlicer"],
): AppConfig["orcaSlicer"] {
  const defaults = defaultAppConfig.orcaSlicer;
  return {
    enabled:
      typeof input?.enabled === "boolean" ? input.enabled : defaults.enabled,
    integrationMode:
      typeof input?.integrationMode === "string" &&
      (input.integrationMode === "native" ||
        input.integrationMode === "web")
        ? input.integrationMode
        : defaults.integrationMode,
    binaryPath:
      typeof input?.binaryPath === "string"
        ? input.binaryPath
        : defaults.binaryPath,
    webUrl:
      typeof input?.webUrl === "string"
        ? input.webUrl
        : defaults.webUrl,
  };
}

function cloneConfig(config: AppConfig): AppConfig {
  return JSON.parse(JSON.stringify(config)) as AppConfig;
}

function mergeAppConfig(input: Partial<AppConfig>): AppConfig {
  const config = {
    ...defaultAppConfig,
    ...input,
    viewport: {
      ...defaultAppConfig.viewport,
      ...input.viewport,
    },
    ai: normalizeAiConfig(input.ai),
    orcaSlicer: normalizeOrcaSlicerConfig(input.orcaSlicer),
    hotkeys: {
      ...defaultAppConfig.hotkeys,
      ...input.hotkeys,
      global: {
        ...defaultAppConfig.hotkeys.global,
        ...input.hotkeys?.global,
      },
      toolbar: {
        ...defaultAppConfig.hotkeys.toolbar,
        ...input.hotkeys?.toolbar,
      },
      viewport: {
        ...defaultAppConfig.hotkeys.viewport,
        ...input.hotkeys?.viewport,
      },
      sketchToolbar: {
        ...defaultAppConfig.hotkeys.sketchToolbar,
        ...input.hotkeys?.sketchToolbar,
      },
    },
  };
  return {
    ...config,
    viewport: {
      ...config.viewport,
      crosshair: isCrosshairMode(config.viewport.crosshair)
        ? config.viewport.crosshair
        : defaultAppConfig.viewport.crosshair,
      showGrid:
        typeof config.viewport.showGrid === "boolean"
          ? config.viewport.showGrid
          : defaultAppConfig.viewport.showGrid,
      showSketchGrid:
        typeof config.viewport.showSketchGrid === "boolean"
          ? config.viewport.showSketchGrid
          : defaultAppConfig.viewport.showSketchGrid,
    },
  };
}

function isThemeConfig(value: unknown): value is ThemeConfig {
  if (!value || typeof value !== "object") {
    return false;
  }

  const candidate = value as Partial<ThemeConfig>;
  return (
    typeof candidate.id === "string" &&
    typeof candidate.name === "string" &&
    Boolean(candidate.colors) &&
    typeof candidate.colors === "object"
  );
}

function themeMapFromList(themes: unknown[]): Record<string, ThemeConfig> {
  const result: Record<string, ThemeConfig> = {};
  for (const theme of themes) {
    if (isThemeConfig(theme)) {
      result[theme.id] = theme;
    }
  }
  return result;
}

function normalizeConfig(
  input: Partial<AppConfig>,
  themes: Record<string, ThemeConfig> = bundledThemes,
): AppConfig {
  const config = mergeAppConfig(input);
  if (!isThemeAvailable(config.theme, themes)) {
    return {
      ...config,
      theme: defaultAppConfig.theme,
    };
  }
  return config;
}

function loadLocalAppConfig(): AppConfig {
  if (typeof window === "undefined") {
    return cloneConfig(defaultAppConfig);
  }

  const stored = window.localStorage.getItem(CONFIG_STORAGE_KEY);
  if (!stored) {
    return cloneConfig(defaultAppConfig);
  }

  try {
    return normalizeConfig(JSON.parse(stored) as Partial<AppConfig>);
  } catch {
    return cloneConfig(defaultAppConfig);
  }
}

function saveLocalAppConfig(config: AppConfig): void {
  if (typeof window === "undefined") {
    return;
  }
  window.localStorage.setItem(CONFIG_STORAGE_KEY, JSON.stringify(config));
}

export function loadAppConfig(): AppConfig {
  return loadLocalAppConfig();
}

export async function bootstrapAppConfig(): Promise<AppConfigBootstrap> {
  try {
    const bootstrap = await invoke<NativeConfigBootstrap>(
      "bootstrap_app_config",
      {
        defaultConfig,
        defaultThemes: DEFAULT_THEME_FILES,
      },
    );
    const themes = themeMapFromList(bootstrap.themes);
    const usableThemes =
      Object.keys(themes).length > 0 ? themes : { ...bundledThemes };
    return {
      config: normalizeConfig(bootstrap.config, usableThemes),
      themes: usableThemes,
      configPath: bootstrap.config_path,
      themesPath: bootstrap.themes_path,
    };
  } catch {
    return {
      config: loadLocalAppConfig(),
      themes: { ...bundledThemes },
      configPath: null,
      themesPath: null,
    };
  }
}

export async function saveAppConfig(config: AppConfig): Promise<void> {
  try {
    await invoke("save_app_config", { config });
  } catch {
    saveLocalAppConfig(config);
  }
}

export function getActiveTheme(
  config: AppConfig = defaultAppConfig,
  themes: Record<string, ThemeConfig> = bundledThemes,
): ThemeConfig {
  const themeId =
    config.theme === SYSTEM_THEME_ID ? getSystemThemeId() : config.theme;
  return themes[themeId] ?? bundledThemes.dark;
}

export function applyTheme(theme: ThemeConfig = getActiveTheme()): void {
  if (typeof document === "undefined") {
    return;
  }

  const root = document.documentElement;
  root.dataset.theme = theme.id;
  for (const [token, value] of Object.entries(theme.colors)) {
    root.style.setProperty(token, value);
  }
}

export function formatHotkey(binding: HotkeyBinding): string {
  const parts: string[] = [];
  if (binding.ctrlOrMeta) {
    parts.push(navigator.platform.toLowerCase().includes("mac") ? "Cmd" : "Ctrl");
  }
  if (binding.shift) {
    parts.push("Shift");
  }
  if (binding.alt) {
    parts.push("Alt");
  }
  parts.push(binding.label);
  return parts.join("+");
}

export function matchesHotkey(event: KeyboardEvent, binding: HotkeyBinding) {
  if (event.code !== binding.code) {
    return false;
  }

  if (Boolean(binding.ctrlOrMeta) !== (event.metaKey || event.ctrlKey)) {
    return false;
  }

  return (
    Boolean(binding.shift) === event.shiftKey &&
    Boolean(binding.alt) === event.altKey
  );
}
