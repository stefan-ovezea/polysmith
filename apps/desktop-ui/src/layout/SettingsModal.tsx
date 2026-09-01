import { useEffect, useMemo, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import type { TFunction } from "i18next";
import { useTranslation } from "react-i18next";
import {
  defaultAppConfig,
  formatHotkey,
  isThemeAvailable,
  useAppConfig,
} from "@/config";
import type { AppConfig, HotkeyBinding, OrcaIntegrationMode } from "@/config";
import { Checkbox, Dropdown, getAiSettings, listAiModels } from "@/lib";
import { usePluginHost } from "@/plugins/PluginProvider";

type SettingsSection =
  | "general"
  | "appearance"
  | "keybinds"
  | "ai"
  | "orca"
  | "plugins";
type LanguageCode = "en" | "es" | "ja" | "zh";

interface SettingsModalProps {
  onClose: () => void;
}

interface HotkeyRow {
  group: keyof AppConfig["hotkeys"];
  key: string;
  label: string;
  binding: HotkeyBinding;
}

type AiModelLoadStatus =
  | { kind: "loading"; message: string }
  | { kind: "success"; message: string }
  | { kind: "error"; message: string };

const RESERVED_HOTKEYS = new Set([
  "mod+KeyC",
  "mod+KeyV",
  "mod+KeyX",
  "mod+KeyQ",
  "mod+KeyW",
  "mod+KeyN",
  "mod+KeyO",
  "mod+KeyS",
  "mod+KeyP",
  "mod+KeyR",
  "mod+Comma",
]);

const BLOCKED_KEY_CODES = new Set([
  "AltLeft",
  "AltRight",
  "ControlLeft",
  "ControlRight",
  "MetaLeft",
  "MetaRight",
  "ShiftLeft",
  "ShiftRight",
  "CapsLock",
  "Tab",
  "Escape",
  "Enter",
]);

function labelFromKeyboardEvent(event: KeyboardEvent) {
  if (event.code === "Space") {
    return "Space";
  }
  if (event.key.length === 1) {
    return event.key.toUpperCase();
  }
  return event.key;
}

function bindingFromKeyboardEvent(event: KeyboardEvent): HotkeyBinding {
  return {
    code: event.code,
    label: labelFromKeyboardEvent(event),
    ctrlOrMeta: event.metaKey || event.ctrlKey || undefined,
    shift: event.shiftKey || undefined,
    alt: event.altKey || undefined,
  };
}

function hotkeySignature(binding: HotkeyBinding) {
  return [
    binding.ctrlOrMeta ? "mod" : "",
    binding.alt ? "alt" : "",
    binding.shift ? "shift" : "",
    binding.code,
  ]
    .filter(Boolean)
    .join("+");
}

function rowId(row: HotkeyRow) {
  return `${row.group}.${row.key}`;
}

function validateHotkey(
  binding: HotkeyBinding,
  row: HotkeyRow,
  rows: HotkeyRow[],
  t: TFunction,
) {
  if (BLOCKED_KEY_CODES.has(binding.code)) {
    return t("settings.hotkeyUseAllowedKey");
  }

  if (binding.shift && !binding.ctrlOrMeta && !binding.alt) {
    return t("settings.hotkeyShiftOnly");
  }

  if (RESERVED_HOTKEYS.has(hotkeySignature(binding))) {
    return t("settings.hotkeyReserved");
  }

  const conflict = rows.find(
    (entry) =>
      rowId(entry) !== rowId(row) &&
      hotkeySignature(entry.binding) === hotkeySignature(binding),
  );
  if (conflict) {
    return t("settings.hotkeyConflict", { label: conflict.label });
  }

  return null;
}

function getDefaultHotkey(row: HotkeyRow): HotkeyBinding {
  const groupDefaults = defaultAppConfig.hotkeys[row.group];
  return groupDefaults[row.key as keyof typeof groupDefaults] as HotkeyBinding;
}

function StatusCheckIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-4 w-4"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2.2"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="m5 12 4 4 10-10" />
    </svg>
  );
}

function StatusXIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-4 w-4"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="2.2"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M6 6l12 12" />
      <path d="M18 6 6 18" />
    </svg>
  );
}

export function SettingsModal({ onClose }: SettingsModalProps) {
  const { i18n, t } = useTranslation();
  const {
    config,
    availableThemes,
    configPath,
    themesPath,
    setConfig,
    updateConfig,
  } = useAppConfig();
  const pluginHost = usePluginHost();
  const [section, setSection] = useState<SettingsSection>("general");
  const [activeHotkeyId, setActiveHotkeyId] = useState<string | null>(null);
  // "unknown" while the ~/.polysmith key lookup is in flight.
  const [aiKeyStatus, setAiKeyStatus] = useState<
    "unknown" | "found" | "missing" | "commandError"
  >("unknown");
  const [aiKeySourcePath, setAiKeySourcePath] = useState("");

  useEffect(() => {
    if (config.ai.provider !== "deepseek") {
      return;
    }
    let cancelled = false;
    void getAiSettings().then((settings) => {
      if (cancelled) {
        return;
      }
      setAiKeySourcePath(settings.sourcePath);
      setAiKeyStatus(
        settings.commandError
          ? "commandError"
          : settings.apiKey
            ? "found"
            : "missing",
      );
    });
    return () => {
      cancelled = true;
    };
  }, [config.ai.provider]);
  const [hotkeyError, setHotkeyError] = useState<string | null>(null);
  const [isConfirmingResetAll, setIsConfirmingResetAll] = useState(false);
  const [aiModelLoadStatus, setAiModelLoadStatus] =
    useState<AiModelLoadStatus | null>(null);
  const [aiModelNames, setAiModelNames] = useState<string[]>([]);
  const [isLoadingAiModels, setIsLoadingAiModels] = useState(false);
  const language = (
    ["en", "es", "ja", "zh"].includes(i18n.resolvedLanguage ?? "")
      ? i18n.resolvedLanguage
      : "en"
  ) as LanguageCode;
  // Language labels are always in English so non-English speakers who
  // accidentally switch away can still navigate back to a known option.
  const languageOptions: Array<{ value: LanguageCode; label: string }> = [
    { value: "en", label: "English" },
    { value: "es", label: "Español" },
    { value: "ja", label: "日本語" },
    { value: "zh", label: "中文" },
  ];
  const aiModelOptions = useMemo(() => {
    const options = aiModelNames.map((modelName) => ({
      value: modelName,
      label: modelName,
    }));
    const configuredModel = config.ai.model.trim();
    if (
      configuredModel &&
      !options.some((option) => option.value === configuredModel)
    ) {
      options.unshift({
        value: configuredModel,
        label: t("settings.modelNotListed", { model: configuredModel }),
      });
    }
    if (options.length === 0) {
      options.push({
        value: "",
        label: t("settings.noModelsLoaded"),
      });
    }
    return options;
  }, [aiModelNames, config.ai.model, t]);

  const hotkeyRows: HotkeyRow[] = [
    {
      group: "global",
      key: "undo",
      label: t("settingsHotkeys.undo"),
      binding: config.hotkeys.global.undo,
    },
    {
      group: "global",
      key: "redo",
      label: t("settingsHotkeys.redo"),
      binding: config.hotkeys.global.redo,
    },
    {
      group: "toolbar",
      key: "extrude",
      label: t("settingsHotkeys.extrude"),
      binding: config.hotkeys.toolbar.extrude,
    },
    {
      group: "toolbar",
      key: "fillet",
      label: t("settingsHotkeys.fillet"),
      binding: config.hotkeys.toolbar.fillet,
    },
    {
      group: "toolbar",
      key: "project",
      label: t("settingsHotkeys.project"),
      binding: config.hotkeys.toolbar.project,
    },
    {
      group: "viewport",
      key: "toggleGrid",
      label: t("settingsHotkeys.toggleGrid"),
      binding: config.hotkeys.viewport.toggleGrid,
    },
    {
      group: "sketchToolbar",
      key: "createSketch",
      label: t("settingsHotkeys.createSketch"),
      binding: config.hotkeys.sketchToolbar.createSketch,
    },
    {
      group: "sketchToolbar",
      key: "line",
      label: t("settingsHotkeys.sketchLine"),
      binding: config.hotkeys.sketchToolbar.line,
    },
    {
      group: "sketchToolbar",
      key: "rectangle",
      label: t("settingsHotkeys.sketchRectangle"),
      binding: config.hotkeys.sketchToolbar.rectangle,
    },
    {
      group: "sketchToolbar",
      key: "circle",
      label: t("settingsHotkeys.sketchCircle"),
      binding: config.hotkeys.sketchToolbar.circle,
    },
    {
      group: "sketchToolbar",
      key: "dimension",
      label: t("settingsHotkeys.sketchDimension"),
      binding: config.hotkeys.sketchToolbar.dimension,
    },
    {
      group: "sketchToolbar",
      key: "toggleConstruction",
      label: t("settingsHotkeys.toggleConstruction"),
      binding: config.hotkeys.sketchToolbar.toggleConstruction,
    },
  ];

  function updateHotkey(row: HotkeyRow, binding: HotkeyBinding) {
    setHotkeyError(null);
    updateConfig((current) => ({
      ...current,
      hotkeys: {
        ...current.hotkeys,
        [row.group]: {
          ...current.hotkeys[row.group],
          [row.key]: binding,
        },
      },
    }));
  }

  function captureHotkey(row: HotkeyRow, event: KeyboardEvent) {
    event.preventDefault();
    event.stopPropagation();

    if (event.code === "Escape") {
      setActiveHotkeyId(null);
      setHotkeyError(null);
      return;
    }

    const binding = bindingFromKeyboardEvent(event);
    const error = validateHotkey(binding, row, hotkeyRows, t);
    if (error) {
      setHotkeyError(error);
      return;
    }

    updateHotkey(row, binding);
    setActiveHotkeyId(null);
  }

  async function loadAiModels() {
    setIsLoadingAiModels(true);
    setAiModelLoadStatus({
      kind: "loading",
      message: t("settings.loadingModels"),
    });
    try {
      const models = await listAiModels(config.ai);
      setAiModelNames(models);
      if (models.length > 0 && !models.includes(config.ai.model.trim())) {
        updateConfig((current) => ({
          ...current,
          ai: {
            ...current.ai,
            model: models[0],
          },
        }));
      }
      setAiModelLoadStatus({
        kind: "success",
        message:
          models.length > 0
            ? t("settings.modelsLoaded", {
                count: models.length,
                plural: models.length === 1 ? "" : "s",
              })
            : t("settings.noModelsFound"),
      });
    } catch (error) {
      setAiModelNames([]);
      setAiModelLoadStatus({
        kind: "error",
        message: t("settings.modelLoadFailed", { error: String(error) }),
      });
    } finally {
      setIsLoadingAiModels(false);
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/55 px-6 py-8 backdrop-blur-sm">
      <section className="cad-floating-panel grid h-[min(680px,calc(100vh-64px))] min-h-0 w-[min(920px,calc(100vw-48px))] grid-cols-[220px_minmax(0,1fr)] overflow-hidden p-0">
        <aside className="border-r border-white/10 bg-black/15 p-3">
          <div className="px-2 py-2">
            <p className="cad-kicker">{t("settings.title")}</p>
          </div>
          <nav className="mt-3 flex flex-col gap-1">
            {[
              ["general", t("settings.general")],
              ["appearance", t("settings.appearance")],
              ["keybinds", t("settings.keybinds")],
              ["ai", t("common.ai")],
              ["plugins", t("header.plugins")],
              ["orca", t("settings.slicer")],
            ].map(([id, label]) => (
              <button
                key={id}
                type="button"
                className={
                  section === id
                    ? "rounded-md bg-white/10 px-3 py-2 text-left text-sm text-on-surface"
                    : "rounded-md px-3 py-2 text-left text-sm text-on-surface-muted transition-colors hover:bg-white/[0.04] hover:text-on-surface"
                }
                onClick={() => setSection(id as SettingsSection)}
              >
                {label}
              </button>
            ))}
          </nav>
        </aside>

        <div className="flex min-h-0 min-w-0 flex-col">
          <header className="flex items-center justify-between gap-4 border-b border-white/10 px-5 py-4">
            <h2 className="font-display text-lg text-on-surface">
              {section === "appearance"
                ? t("settings.appearance")
                : section === "general"
                  ? t("settings.general")
                  : section === "keybinds"
                  ? t("settings.keybinds")
                  : section === "ai"
                    ? t("common.ai")
                    : section === "plugins"
                      ? t("header.plugins")
                      : t("settings.slicer")}
            </h2>
          </header>

          <div className="cad-scrollbar min-h-0 flex-1 overflow-auto p-5">
            {section === "general" ? (
              <div className="block max-w-sm">
                <span className="cad-kicker">{t("settings.language")}</span>
                <Dropdown
                  label={t("settings.language")}
                  className="mt-3 w-full"
                  value={language}
                  options={languageOptions}
                  onChange={(nextLanguage) => {
                    void i18n.changeLanguage(nextLanguage);
                  }}
                />
              </div>
            ) : section === "appearance" ? (
              <div className="block max-w-sm">
                <span className="cad-kicker">{t("settings.theme")}</span>
                <Dropdown
                  label={t("settings.theme")}
                  className="mt-3 w-full"
                  value={config.theme}
                  options={availableThemes.map((theme) => ({
                    value: theme.id,
                    label: theme.name,
                  }))}
                  onChange={(theme) => {
                    const themeMap = Object.fromEntries(
                      availableThemes.map((entry) => [entry.id, entry]),
                    );
                    if (!isThemeAvailable(theme, themeMap)) {
                      return;
                    }
                    updateConfig((current) => ({
                      ...current,
                      theme,
                    }));
                  }}
                />
                {configPath && themesPath ? (
                  <div className="mt-4 space-y-1 text-xs text-on-surface-muted">
                    <p>Config: {configPath}</p>
                    <p>Themes: {themesPath}</p>
                  </div>
                ) : null}
              </div>
            ) : section === "keybinds" ? (
              <div className="space-y-2">
                {hotkeyError ? (
                  <p className="rounded-md border border-red-400/30 bg-red-500/10 px-3 py-2 text-sm text-red-200">
                    {hotkeyError}
                  </p>
                ) : null}
                {hotkeyRows.map((row) => (
                  <div
                    key={`${row.group}.${row.key}`}
                    className="grid grid-cols-[minmax(0,1fr)_180px_auto] items-center gap-3 rounded-md border border-white/10 bg-white/[0.025] px-3 py-2"
                  >
                    <span className="text-sm text-on-surface">
                      {row.label}
                    </span>
                    <button
                      type="button"
                      aria-label={t("settings.setHotkey", {
                        label: row.label,
                      })}
                      className={
                        activeHotkeyId === rowId(row)
                          ? "cad-ribbon-action justify-center border-primary-edge text-on-surface"
                          : "cad-ribbon-action justify-center"
                      }
                      onClick={() => {
                        setActiveHotkeyId(rowId(row));
                        setHotkeyError(null);
                      }}
                      onKeyDown={(event) => {
                        if (activeHotkeyId !== rowId(row)) {
                          return;
                        }
                        captureHotkey(row, event.nativeEvent);
                      }}
                    >
                      {activeHotkeyId === rowId(row)
                        ? t("settings.pressShortcut")
                        : formatHotkey(row.binding)}
                    </button>
                    <button
                      type="button"
                      className="cad-ribbon-action"
                      onClick={() => {
                        const binding = getDefaultHotkey(row);
                        const error = validateHotkey(binding, row, hotkeyRows, t);
                        if (error) {
                          setHotkeyError(error);
                          return;
                        }
                        updateHotkey(row, binding);
                        setActiveHotkeyId(null);
                      }}
                    >
                      {t("common.reset")}
                    </button>
                  </div>
                ))}
              </div>
            ) : section === "ai" ? (
              <div className="max-w-xl space-y-5">
                <div className="rounded-md border border-white/10 bg-white/[0.025] px-4 py-4">
                  <label className="flex items-center justify-between gap-4">
                    <span>
                      <span className="block text-sm font-medium text-on-surface">
                        {t("settings.enableAiAssistant")}
                      </span>
                      <span className="mt-1 block text-xs text-on-surface-muted">
                        {t("settings.enableAiAssistantDescription")}
                      </span>
                    </span>
                    <Checkbox
                      checked={config.ai.enabled}
                      ariaLabel={t("settings.enableAiAssistant")}
                      onCheckedChange={(enabled) => {
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            enabled,
                          },
                        }));
                        if (enabled) {
                          // One-click setup: fetch the model list immediately
                          // so a model is auto-selected and ready to use.
                          void loadAiModels();
                        }
                      }}
                    />
                  </label>
                </div>

                <fieldset
                  disabled={!config.ai.enabled}
                  className={
                    config.ai.enabled
                      ? "m-0 space-y-3 border-0 p-0"
                      : "m-0 space-y-3 border-0 p-0 opacity-45"
                  }
                >
                  <label className="block">
                    <span className="cad-kicker">{t("settings.aiProvider")}</span>
                    <Dropdown
                      label={t("settings.aiProvider")}
                      className="mt-2 w-full"
                      value={config.ai.provider}
                      options={[
                        {
                          value: "ollama",
                          label: t("settings.aiProviderOllama"),
                        },
                        {
                          value: "deepseek",
                          label: t("settings.aiProviderDeepseek"),
                        },
                      ]}
                      onChange={(provider) => {
                        const other =
                          provider === "deepseek" ? "ollama" : "deepseek";
                        const defaults = {
                          ollama: "http://localhost:11434",
                          deepseek: "https://api.deepseek.com/anthropic",
                        } as const;
                        setAiModelLoadStatus(null);
                        setAiModelNames([]);
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            provider,
                            // Swap the endpoint default when switching
                            // providers while the URL is still the other
                            // provider's default.
                            baseUrl:
                              !current.ai.baseUrl ||
                              current.ai.baseUrl === defaults[other]
                                ? defaults[provider]
                                : current.ai.baseUrl,
                            // Reset the model too — a model name from the
                            // other provider is meaningless on this one.
                            model:
                              provider === "deepseek"
                                ? "deepseek-v4-flash"
                                : "",
                          },
                        }));
                      }}
                    />
                  </label>

                  <label className="block">
                    <span className="cad-kicker">{t("settings.aiBaseUrl")}</span>
                    <input
                      className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
                      value={config.ai.baseUrl}
                      placeholder={
                        config.ai.provider === "deepseek"
                          ? "https://api.deepseek.com/anthropic"
                          : "http://localhost:11434"
                      }
                      onChange={(event) => {
                        setAiModelLoadStatus(null);
                        setAiModelNames([]);
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            baseUrl: event.target.value,
                          },
                        }));
                      }}
                    />
                  </label>

                  {config.ai.provider === "deepseek" ? (
                    <>
                      <div>
                        <span className="cad-kicker">{t("settings.aiApiKey")}</span>
                        <p className="mt-2 text-sm text-on-surface-muted">
                          {aiKeyStatus === "found"
                            ? t("settings.aiApiKeyFound", {
                                path: aiKeySourcePath,
                              })
                            : aiKeyStatus === "missing"
                              ? t("settings.aiApiKeyMissing", {
                                  path: aiKeySourcePath,
                                })
                              : aiKeyStatus === "commandError"
                                ? t("settings.aiApiKeyCommandError")
                                : t("settings.aiApiKeyChecking")}
                        </p>
                      </div>

                      <label className="block">
                        <span className="cad-kicker">{t("settings.aiApiStyle")}</span>
                        <Dropdown
                          label={t("settings.aiApiStyle")}
                          className="mt-2 w-full"
                          value={config.ai.apiStyle}
                          options={[
                            {
                              value: "anthropic",
                              label: t("settings.aiApiStyleAnthropic"),
                            },
                            {
                              value: "openai",
                              label: t("settings.aiApiStyleOpenai"),
                            },
                          ]}
                          onChange={(apiStyle) => {
                            setAiModelLoadStatus(null);
                            setAiModelNames([]);
                            updateConfig((current) => ({
                              ...current,
                              ai: {
                                ...current.ai,
                                apiStyle,
                                // Each style belongs to its own endpoint
                                // path: swap the deepseek URL defaults with
                                // the style so the pair always matches.
                                baseUrl:
                                  current.ai.baseUrl ===
                                    "https://api.deepseek.com/anthropic" ||
                                  current.ai.baseUrl ===
                                    "https://api.deepseek.com"
                                    ? apiStyle === "openai"
                                      ? "https://api.deepseek.com"
                                      : "https://api.deepseek.com/anthropic"
                                    : current.ai.baseUrl,
                              },
                            }));
                          }}
                        />
                      </label>
                    </>
                  ) : null}

                  <div className="flex flex-wrap items-center gap-3">
                    <button
                      type="button"
                      className="cad-ribbon-action"
                      disabled={isLoadingAiModels || !config.ai.enabled}
                      onClick={() => void loadAiModels()}
                    >
                      {t("settings.loadModels")}
                    </button>
                    {aiModelLoadStatus ? (
                      <span
                        className={`flex items-center gap-1.5 text-sm ${
                          aiModelLoadStatus.kind === "success"
                            ? "text-success"
                            : aiModelLoadStatus.kind === "error"
                              ? "text-danger"
                              : "text-on-surface-muted"
                        }`}
                      >
                        {aiModelLoadStatus.kind === "success" ? (
                          <StatusCheckIcon />
                        ) : aiModelLoadStatus.kind === "error" ? (
                          <StatusXIcon />
                        ) : null}
                        {aiModelLoadStatus.message}
                      </span>
                    ) : null}
                  </div>

                  <label className="block">
                    <span className="cad-kicker">{t("settings.model")}</span>
                    <Dropdown
                      label={t("settings.model")}
                      className="mt-2 w-full"
                      value={config.ai.model}
                      options={aiModelOptions}
                      disabled={!config.ai.enabled || aiModelNames.length === 0}
                      onChange={(model) => {
                        setAiModelLoadStatus(null);
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            model,
                          },
                        }));
                      }}
                    />
                  </label>

                  <label className="block">
                    <span className="cad-kicker">{t("settings.maxAgentSteps")}</span>
                    <input
                      className="mt-2 w-28 rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
                      type="number"
                      min={1}
                      max={10}
                      value={config.ai.maxAgentSteps}
                      onChange={(event) => {
                        const value = Number(event.target.value);
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            maxAgentSteps: Number.isFinite(value) ? value : 5,
                          },
                        }));
                      }}
                    />
                  </label>

                  <label className="flex items-center justify-between gap-4">
                    <span>
                      <span className="block text-sm font-medium text-on-surface">
                        {t("settings.previewBeforeRun")}
                      </span>
                      <span className="mt-1 block text-xs text-on-surface-muted">
                        {t("settings.previewBeforeRunDescription")}
                      </span>
                    </span>
                    <Checkbox
                      checked={config.ai.previewBeforeRun}
                      ariaLabel={t("settings.previewBeforeRun")}
                      onCheckedChange={(previewBeforeRun) => {
                        updateConfig((current) => ({
                          ...current,
                          ai: {
                            ...current.ai,
                            previewBeforeRun,
                          },
                        }));
                      }}
                    />
                  </label>
                </fieldset>
              </div>
            ) : section === "plugins" ? (
              <div className="max-w-2xl space-y-4">
                {pluginHost.configPath ? (
                  <p className="text-xs text-on-surface-muted">
                    {t("plugins.configPath", { path: pluginHost.configPath })}
                  </p>
                ) : null}
                {pluginHost.plugins.map((entry) => (
                  <section
                    key={entry.plugin.manifest.id}
                    className="rounded-md border border-white/10 bg-white/[0.025] p-4"
                  >
                    <label className="flex items-start justify-between gap-4">
                      <span>
                        <span className="block text-sm font-medium text-on-surface">
                          {entry.plugin.manifest.name}
                        </span>
                        <span className="mt-1 block text-xs text-on-surface-muted">
                          {entry.plugin.manifest.description}
                        </span>
                      </span>
                      <Checkbox
                        checked={entry.enabled}
                        ariaLabel={entry.plugin.manifest.name}
                        onCheckedChange={(enabled) =>
                          pluginHost.setPluginEnabled(
                            entry.plugin.manifest.id,
                            enabled,
                          )
                        }
                      />
                    </label>
                    <div className="mt-4 border-t border-white/10 pt-4">
                      {entry.runtime.renderSettings({
                        config: entry.config,
                        disabled: !entry.enabled,
                        onChange: (nextConfig) =>
                          pluginHost.updatePluginConfig(
                            entry.plugin.manifest.id,
                            () => nextConfig,
                          ),
                      })}
                    </div>
                  </section>
                ))}
              </div>
            ) : (
              <div className="max-w-xl space-y-5">
                <div className="rounded-md border border-white/10 bg-white/[0.025] px-4 py-4">
                  <label className="flex items-center justify-between gap-4">
                    <span>
                      <span className="block text-sm font-medium text-on-surface">
                        {t("settings.enableEmbeddedOrcaSlicer")}
                      </span>
                      <span className="mt-1 block text-xs text-on-surface-muted">
                        {t("settings.enableEmbeddedOrcaSlicerDescription")}
                      </span>
                    </span>
                    <Checkbox
                      checked={config.orcaSlicer.enabled}
                      ariaLabel={t("settings.enableEmbeddedOrcaSlicer")}
                      onCheckedChange={(enabled) => {
                        updateConfig((current) => ({
                          ...current,
                          orcaSlicer: {
                            ...current.orcaSlicer,
                            enabled,
                          },
                        }));
                      }}
                    />
                  </label>
                </div>

                <fieldset
                  disabled={!config.orcaSlicer.enabled}
                  className={
                    config.orcaSlicer.enabled
                      ? "m-0 space-y-5 border-0 p-0"
                      : "m-0 space-y-5 border-0 p-0 opacity-45"
                  }
                >
                  <label className="block">
                    <span className="cad-kicker">
                      {t("settings.orcaSlicerIntegrationMode")}
                    </span>
                    <div className="mt-2">
                      <Dropdown
                        value={config.orcaSlicer.integrationMode}
                        options={[
                          {
                            value: "native",
                            label: t(
                              "settings.orcaSlicerIntegrationModeNative",
                            ),
                          },
                          {
                            value: "external",
                            label: t(
                              "settings.orcaSlicerIntegrationModeExternal",
                            ),
                          },
                          {
                            value: "web",
                            label: t(
                              "settings.orcaSlicerIntegrationModeWeb",
                            ),
                          },
                        ]}
                        label={t("settings.orcaSlicerIntegrationMode")}
                        disabled={!config.orcaSlicer.enabled}
                        onChange={(value) => {
                          updateConfig((current) => ({
                            ...current,
                            orcaSlicer: {
                              ...current.orcaSlicer,
                              integrationMode: value as OrcaIntegrationMode,
                            },
                          }));
                        }}
                      />
                    </div>
                  </label>

                  {config.orcaSlicer.integrationMode !== "web" ? (
                    <label className="block">
                      <span className="cad-kicker">
                        {t("settings.orcaSlicerBinaryPath")}
                      </span>
                      <div className="mt-2 flex gap-2">
                        <input
                          className="min-w-0 flex-1 rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
                          value={config.orcaSlicer.binaryPath}
                          placeholder={t(
                            "settings.orcaSlicerBinaryPlaceholder",
                          )}
                          onChange={(event) => {
                            updateConfig((current) => ({
                              ...current,
                              orcaSlicer: {
                                ...current.orcaSlicer,
                                binaryPath: event.target.value,
                              },
                            }));
                          }}
                        />
                        <button
                          type="button"
                          className="cad-ribbon-action"
                          onClick={async () => {
                            const selectedPath = await open({
                              title: t("settings.selectOrcaSlicerBinary"),
                              multiple: false,
                              directory: false,
                            });
                            if (typeof selectedPath !== "string") {
                              return;
                            }
                            updateConfig((current) => ({
                              ...current,
                              orcaSlicer: {
                                ...current.orcaSlicer,
                                binaryPath: selectedPath,
                              },
                            }));
                          }}
                        >
                          {t("common.browse")}
                        </button>
                      </div>
                    </label>
                  ) : (
                    <label className="block">
                      <span className="cad-kicker">
                        {t("settings.orcaSlicerWebUrl")}
                      </span>
                      <div className="mt-2">
                        <input
                          className="w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
                          value={config.orcaSlicer.webUrl}
                          placeholder={t(
                            "settings.orcaSlicerWebUrlPlaceholder",
                          )}
                          onChange={(event) => {
                            updateConfig((current) => ({
                              ...current,
                              orcaSlicer: {
                                ...current.orcaSlicer,
                                webUrl: event.target.value,
                              },
                            }));
                          }}
                        />
                      </div>
                    </label>
                  )}
                </fieldset>
              </div>
            )}
          </div>

          <footer className="flex items-center justify-end gap-3 border-t border-white/10 px-5 py-3">
            {isConfirmingResetAll ? (
              <>
                <span className="mr-auto text-sm text-on-surface-muted">
                  {t("settings.resetAllQuestion")}
                </span>
                <button
                  type="button"
                  className="cad-ribbon-action"
                  onClick={() => setIsConfirmingResetAll(false)}
                >
                  {t("common.cancel")}
                </button>
                <button
                  type="button"
                  className="cad-ribbon-action"
                  onClick={() => {
                    setConfig(defaultAppConfig);
                    setActiveHotkeyId(null);
                    setHotkeyError(null);
                    setIsConfirmingResetAll(false);
                  }}
                >
                  {t("settings.confirmReset")}
                </button>
              </>
            ) : (
              <button
                type="button"
                className="cad-ribbon-action"
                onClick={() => setIsConfirmingResetAll(true)}
              >
                {t("settings.resetAll")}
              </button>
            )}
            <button
              type="button"
              className="cad-ribbon-action"
              onClick={onClose}
            >
              {t("common.close")}
            </button>
          </footer>
        </div>
      </section>
    </div>
  );
}
