import type { LogEntry, LogLevel } from "@/types";
import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";

interface LogsWindowProps {
  logs: LogEntry[];
  onClose: () => void;
  onClear: () => void;
}

const levelClasses: Record<LogLevel, string> = {
  debug: "text-sky-200",
  info: "text-on-surface",
  warn: "text-amber-300",
  error: "text-danger",
};

function formatLogTime(timestamp: string) {
  const date = new Date(timestamp);
  if (Number.isNaN(date.getTime())) {
    return timestamp;
  }
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

export function LogsWindow({ logs, onClose, onClear }: LogsWindowProps) {
  const { t } = useTranslation();
  // Debug entries (e.g. per-pointer-move trim diagnostics) flood the
  // panel — hide them by default, opt in via the toggle.
  const [showDebug, setShowDebug] = useState(false);
  const visibleLogs = useMemo(
    () => (showDebug ? logs : logs.filter((entry) => entry.level !== "debug")),
    [logs, showDebug],
  );
  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        onClose();
      }
    }
    window.addEventListener("keydown", handleKeyDown, true);
    return () => {
      window.removeEventListener("keydown", handleKeyDown, true);
    };
  }, [onClose]);

  return (
    <div className="pointer-events-auto fixed inset-x-6 top-24 z-40 mx-auto max-w-5xl">
      <section className="cad-floating-panel overflow-hidden p-0 shadow-[0_18px_70px_rgba(0,0,0,0.48)]">
        <div className="flex items-center justify-between gap-4 border-b border-white/10 px-4 py-3">
          <div>
            <p className="cad-kicker">{t("logs.title")}</p>
            <p className="mt-1 text-xs text-on-surface-dim">
              {t("common.entries", { count: visibleLogs.length })}
            </p>
          </div>
          <div className="flex items-center gap-2">
            <button
              type="button"
              className="cad-ribbon-action"
              onClick={() => setShowDebug((value) => !value)}
            >
              {showDebug ? t("logs.hideDebug") : t("logs.showDebug")}
            </button>
            <button
              type="button"
              className="cad-ribbon-action"
              onClick={onClear}
              disabled={logs.length === 0}
            >
              {t("common.clear")}
            </button>
            <button
              type="button"
              className="cad-ribbon-action"
              onClick={onClose}
              aria-label={t("logs.close")}
            >
              {t("common.close")}
            </button>
          </div>
        </div>
        <div className="cad-scrollbar max-h-[min(520px,calc(100vh-190px))] overflow-auto">
          {visibleLogs.length === 0 ? (
            <div className="px-4 py-10 text-sm text-on-surface-muted">
              {t("logs.noLogs")}
            </div>
          ) : (
            <table className="w-full table-fixed border-collapse text-left text-xs">
              <thead className="sticky top-0 bg-surface/95 text-on-surface-dim backdrop-blur">
                <tr>
                  <th className="w-24 px-4 py-2 font-medium">{t("logs.time")}</th>
                  <th className="w-20 px-3 py-2 font-medium">{t("logs.level")}</th>
                  <th className="w-28 px-3 py-2 font-medium">{t("logs.source")}</th>
                  <th className="px-3 py-2 font-medium">{t("logs.message")}</th>
                </tr>
              </thead>
              <tbody>
                {visibleLogs.map((entry, index) => (
                  <tr
                    key={`${entry.timestamp}-${entry.source}-${index}`}
                    className="border-t border-white/10 align-top"
                  >
                    <td className="px-4 py-2 text-on-surface-dim">
                      {formatLogTime(entry.timestamp)}
                    </td>
                    <td
                      className={`px-3 py-2 font-medium uppercase ${levelClasses[entry.level]}`}
                    >
                      {entry.level}
                    </td>
                    <td className="px-3 py-2 text-on-surface-muted">
                      {entry.source}
                    </td>
                    <td className="select-text whitespace-pre-wrap break-words px-3 py-2 text-on-surface">
                      {entry.message}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </section>
    </div>
  );
}
