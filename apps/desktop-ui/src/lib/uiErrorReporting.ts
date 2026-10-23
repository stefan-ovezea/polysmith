import { invoke } from "@tauri-apps/api/core";

/**
 * UI-side error reporting that survives the UI.
 *
 * Why this exists: the in-app Logs panel lives inside the React tree. A render
 * failure that unmounts the tree (see `DiagnosticErrorBoundary` in `main.tsx`)
 * removes the panel too, so the one message that matters disappears exactly
 * when it is needed — the window goes blank and the terminal shows nothing.
 *
 * Everything here writes to `<config dir>/ui-errors.log` through the Rust
 * `log_ui_error` command, which outlives the renderer. On Windows that file is
 * `%APPDATA%\polysmith\ui-errors.log`.
 */

const isTauri = (): boolean =>
  typeof window !== "undefined" && "__TAURI_INTERNALS__" in window;

/** Writes one failure to the durable log. Never throws, never rejects. */
export function reportUiError(message: string, stack?: string | null): void {
  if (!isTauri()) {
    return;
  }
  try {
    void invoke("log_ui_error", {
      message: String(message).slice(0, 4000),
      stack: stack ? String(stack).slice(0, 12000) : null,
    }).catch(() => undefined);
  } catch {
    // A failure while reporting a failure must stay silent.
  }
}

export type UiErrorSink = (message: string, stack?: string | null) => void;

/**
 * Captures uncaught exceptions and unhandled promise rejections.
 *
 * `onError` lets the caller mirror the text into the in-app Logs panel while the
 * tree is still alive; the durable file is written either way.
 */
export function installUiErrorCapture(onError?: UiErrorSink): void {
  if (typeof window === "undefined") {
    return;
  }

  const report = (label: string, error: unknown, fallbackStack?: string) => {
    const message =
      error instanceof Error
        ? `${label}: ${error.message}`
        : `${label}: ${String(error)}`;
    const stack =
      error instanceof Error ? error.stack ?? fallbackStack : fallbackStack;
    reportUiError(message, stack);
    onError?.(message, stack);
  };

  window.addEventListener("error", (event) => {
    const errorEvent = event as ErrorEvent;
    report("uncaught error", errorEvent.error ?? errorEvent.message);
  });

  window.addEventListener("unhandledrejection", (event) => {
    report("unhandled rejection", (event as PromiseRejectionEvent).reason);
  });
}

/**
 * F12 / Ctrl+Shift+I opens the WebView devtools.
 *
 * Tauri does not bind these itself — without this, a packaged or even a dev
 * build has no way to reach the browser console at all.
 */
export function installDevtoolsShortcut(): void {
  if (typeof window === "undefined" || !isTauri()) {
    return;
  }
  window.addEventListener(
    "keydown",
    (event) => {
      const isF12 = event.key === "F12";
      const isInspect =
        event.ctrlKey &&
        event.shiftKey &&
        (event.key === "I" || event.key === "i");
      if (!isF12 && !isInspect) {
        return;
      }
      event.preventDefault();
      void invoke("open_devtools").catch(() => undefined);
    },
    true,
  );
}
