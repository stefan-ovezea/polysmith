import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import type { GrblSetting } from "@/lib/grblClient";
import { GRBL_SETTINGS_DESC_KEYS } from "./grblSettingsMeta";

// Modal table of the controller's `$$` settings.  `settings` comes
// from the grbl-store (filled by the "settings" stream event);
// `null` = the dump has not arrived yet.  Each row edits locally and
// applies with `$k=v` — GRBL echoes every change back in the status
// stream, FluidNC may not.
interface GrblSettingsDialogProps {
  settings: GrblSetting[] | null;
  onClose: () => void;
  onApply: (key: string, value: string) => void;
}

export function GrblSettingsDialog({
  settings,
  onClose,
  onApply,
}: GrblSettingsDialogProps) {
  const { t } = useTranslation();

  // Local edits, seeded from the dump when it arrives.
  const [edits, setEdits] = useState<Record<string, string>>({});
  const seededKeys = settings?.map((setting) => setting.key).join(",") ?? "";
  useEffect(() => {
    if (!settings) {
      return;
    }
    setEdits(
      Object.fromEntries(settings.map((setting) => [setting.key, setting.value])),
    );
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [seededKeys, settings === null]);

  // Escape closes, like the floating CAM panels.
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onClose]);

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/40"
      onClick={onClose}
    >
      <div
        className="cad-floating-panel flex max-h-[80vh] w-[520px] max-w-full flex-col px-5 py-5"
        onClick={(event) => {
          // Clicks inside the card must not close it.
          event.stopPropagation();
        }}
      >
        <div className="flex items-center justify-between">
          <p className="cad-kicker">{t("cam.grbl.settingsTitle", "GRBL settings")}</p>
          <button
            type="button"
            className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider"
            onClick={onClose}
          >
            {t("cam.grbl.settingsClose", "Close")}
          </button>
        </div>

        <p className="mt-2 text-[10px] leading-snug text-on-surface-dim">
          {t(
            "cam.grbl.settingsFluidNcCaveat",
            "These $ settings belong to GRBL 1.1. FluidNC keeps most of its configuration in config.yaml — $ may list only a subset or nothing.",
          )}
        </p>

        {settings === null ? (
          <p className="mt-3 font-mono text-[10px] text-on-surface-muted">
            {t("cam.grbl.settingsLoading", "Waiting for the $$ reply…")}
          </p>
        ) : settings.length === 0 ? (
          <p className="mt-3 font-mono text-[10px] text-on-surface-muted">
            {t("cam.grbl.settingsEmpty", "The controller returned no settings.")}
          </p>
        ) : (
          <div className="mt-3 min-h-0 flex-1 space-y-1.5 overflow-y-auto">
            {settings.map((setting) => {
              const descriptionKey = GRBL_SETTINGS_DESC_KEYS[setting.key];
              return (
                <div
                  key={setting.key}
                  className="flex items-center gap-2 rounded-md bg-[var(--cad-panel-soft-bg)] px-2 py-1.5"
                >
                  <span className="w-12 shrink-0 font-mono text-[10px] text-on-surface">
                    {setting.key}
                  </span>
                  <input
                    type="text"
                    className="cad-input min-w-0 flex-1 font-mono text-[10px]"
                    value={edits[setting.key] ?? setting.value}
                    onChange={(event) =>
                      setEdits((previous) => ({
                        ...previous,
                        [setting.key]: event.target.value,
                      }))
                    }
                  />
                  {descriptionKey ? (
                    <span
                      className="w-44 shrink-0 text-[9px] leading-snug text-on-surface-dim"
                      title={t(`cam.grbl.settingsDesc.${descriptionKey}`)}
                    >
                      {t(`cam.grbl.settingsDesc.${descriptionKey}`)}
                    </span>
                  ) : null}
                  <button
                    type="button"
                    className="cad-action-ghost h-7 shrink-0 px-2 text-[10px] uppercase tracking-wider"
                    disabled={(edits[setting.key] ?? setting.value) === setting.value}
                    onClick={() =>
                      onApply(setting.key, edits[setting.key] ?? setting.value)
                    }
                  >
                    {t("cam.grbl.settingsApply", "Apply")}
                  </button>
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
}
