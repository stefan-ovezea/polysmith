export interface HotkeyBinding {
  code: string;
  label: string;
  ctrlOrMeta?: boolean;
  shift?: boolean;
  alt?: boolean;
}

export interface AppHotkeys {
  global: {
    undo: HotkeyBinding;
    redo: HotkeyBinding;
  };
  toolbar: {
    extrude: HotkeyBinding;
    fillet: HotkeyBinding;
    project: HotkeyBinding;
  };
  viewport: {
    toggleGrid: HotkeyBinding;
  };
  sketchToolbar: {
    createSketch: HotkeyBinding;
    line: HotkeyBinding;
    rectangle: HotkeyBinding;
    circle: HotkeyBinding;
    dimension: HotkeyBinding;
    trim: HotkeyBinding;
    move: HotkeyBinding;
    toggleConstruction: HotkeyBinding;
  };
}

export type CrosshairMode =
  | "default"
  | "viewport-25"
  | "viewport-50"
  | "viewport-75"
  | "infinite";

export interface ViewportConfig {
  crosshair: CrosshairMode;
  showGrid: boolean;
  showSketchGrid: boolean;
}

// "deepseek" speaks to api.deepseek.com in either of two API shapes:
// "anthropic" (/anthropic/v1/messages, x-api-key — e.g. deepseek-v4-pro[1m])
// or "openai" (/chat/completions, Bearer — e.g. deepseek-chat).
export type AiApiStyle = "anthropic" | "openai";

export interface AiConfig {
  enabled: boolean;
  provider: "ollama" | "deepseek";
  baseUrl: string;
  model: string;
  apiKey: string;
  apiStyle: AiApiStyle;
  previewBeforeRun: boolean;
  maxAgentSteps: number;
}

export type OrcaIntegrationMode = "native" | "web";

export interface OrcaSlicerConfig {
  enabled: boolean;
  integrationMode: OrcaIntegrationMode;
  binaryPath: string;
  webUrl: string;
}

export type ThemeSelection =
  | "system"
  | "dark"
  | "light"
  | "catppuccin-latte"
  | "catppuccin-frappe"
  | "catppuccin-macchiato"
  | "catppuccin-mocha"
  | (string & {});

export type DisplayUnits = "mm" | "in";

export interface AppConfig {
  theme: ThemeSelection;
  hotkeys: AppHotkeys;
  viewport: ViewportConfig;
  ai: AiConfig;
  orcaSlicer: OrcaSlicerConfig;
  displayUnits: DisplayUnits;
}

export interface ThemeConfig {
  id: string;
  name: string;
  colors: Record<string, string>;
}
