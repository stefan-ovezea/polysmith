import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import { AppConfigProvider, useAppConfig } from "./config";
import { showMainWindow } from "./lib/windowLifecycle";
import { makeUiLogEntry, writeLogToConsole } from "./lib/logger";
import {
  installDevtoolsShortcut,
  installUiErrorCapture,
  reportUiError,
} from "./lib/uiErrorReporting";
import { PluginProvider } from "./plugins/PluginProvider";
import { useCadCoreStore } from "./state";
import "./i18n";
import "./styles.css";

// Catches render-loop and render-time errors and logs the COMPONENT STACK —
// the exact render path that failed.
//
// This boundary is also why a hard render failure shows a *blank* window: it
// used to render `<div />`, and the Logs panel lives inside the tree it
// replaces, so the message vanished with the UI. The error is now (a) written
// to `<config dir>/ui-errors.log` through `reportUiError`, which survives the
// renderer, and (b) rendered in place in dev builds so it is readable without
// devtools.
class DiagnosticErrorBoundary extends React.Component<
  { children: React.ReactNode },
  { failed: boolean; message: string }
> {
  constructor(props: { children: React.ReactNode }) {
    super(props);
    this.state = { failed: false, message: "" };
  }

  static getDerivedStateFromError(error: unknown) {
    return {
      failed: true,
      message: error instanceof Error ? error.message : String(error),
    };
  }

  componentDidCatch(error: unknown, errorInfo: React.ErrorInfo) {
    // `componentStack` lives on the second argument — reading it off the error
    // object (as this used to) always produced "(none)".
    const componentStack = errorInfo?.componentStack ?? "";
    const entry = makeUiLogEntry(
      "error",
      "desktop_ui",
      `render error: ${String(error).slice(0, 2000)}\ncomponent stack:\n${
        componentStack.slice(0, 6000) || "(none)"
      }`,
    );
    writeLogToConsole(entry);
    useCadCoreStore.getState().addLogEntry(entry);
    reportUiError(`render error: ${String(error)}`, componentStack);
  }

  render() {
    if (!this.state.failed) {
      return this.props.children;
    }
    if (!import.meta.env.DEV) {
      return <div />;
    }
    return (
      <pre
        data-testid="render-error"
        style={{
          margin: 0,
          padding: 16,
          height: "100vh",
          overflow: "auto",
          whiteSpace: "pre-wrap",
          font: "12px/1.5 monospace",
          color: "#f8d0d0",
          background: "#1a1012",
        }}
      >
        {`A render error unmounted the UI.\n\n${this.state.message}\n\nThe full component stack was appended to %APPDATA%\\polysmith\\ui-errors.log\nPress F12 for devtools (dev builds).`}
      </pre>
    );
  }
}

const startupStartedAt = performance.now();
const minimumSplashDurationMs = 350;

// [DIAGNOSTIC — temporary] Forward React dev warnings (e.g. "Cannot
// update a component while rendering a different component") into the
// in-app Logs panel — they name the component pair involved in the
// render loop before React throws "Maximum update depth exceeded".
// Removed once the loop is fixed.
{
  const forward = (level: "error" | "warn", args: unknown[]) => {
    try {
      const text = args.map(String).join(" ").slice(0, 2000);
      useCadCoreStore.getState().addLogEntry(
        makeUiLogEntry(level, "react_warning", text),
      );
    } catch {
      // store not ready yet — ignore
    }
  };
  const originalError = console.error.bind(console);
  const originalWarn = console.warn.bind(console);
  console.error = (...args: unknown[]) => {
    originalError(...args);
    forward("error", args);
  };
  console.warn = (...args: unknown[]) => {
    originalWarn(...args);
    forward("warn", args);
  };
}

function waitForPaint() {
  return new Promise<void>((resolve) => {
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(() => resolve());
    });
  });
}

function waitForMinimumSplashDuration() {
  const elapsedMs = performance.now() - startupStartedAt;
  const remainingMs = Math.max(0, minimumSplashDurationMs - elapsedMs);
  if (remainingMs === 0) {
    return Promise.resolve();
  }

  return new Promise<void>((resolve) => {
    window.setTimeout(resolve, remainingMs);
  });
}

async function waitForFrontendShell() {
  await waitForMinimumSplashDuration();
  await waitForPaint();
}

function StartupReveal() {
  const { isConfigReady } = useAppConfig();

  React.useEffect(() => {
    if (!isConfigReady) {
      return undefined;
    }

    let isCancelled = false;
    let retry: number | null = null;

    void waitForFrontendShell().then(() => {
      if (isCancelled) {
        return;
      }

      void showMainWindow().catch((error) => {
        console.error("failed to show main window", error);
      });
      retry = window.setInterval(() => {
        void showMainWindow()
          .then(() => {
            if (retry !== null) {
              window.clearInterval(retry);
              retry = null;
            }
          })
          .catch(() => undefined);
      }, 500);
    });

    return () => {
      isCancelled = true;
      if (retry !== null) {
        window.clearInterval(retry);
      }
    };
  }, [isConfigReady]);

  return null;
}

function PluginShell() {
  const document = useCadCoreStore((state) => state.document);
  const viewport = useCadCoreStore((state) => state.viewport);

  return (
    <PluginProvider document={document} viewport={viewport}>
      <App />
    </PluginProvider>
  );
}

// F12 / Ctrl+Shift+I open the WebView devtools (Tauri binds neither), and
// uncaught errors are mirrored into the durable UI error log so a blank window
// still leaves a message behind.
installDevtoolsShortcut();
installUiErrorCapture((message) => {
  // Durable log is written by the capture itself; this mirrors the text into
  // the in-app Logs panel while the tree is still alive.
  try {
    useCadCoreStore
      .getState()
      .addLogEntry(makeUiLogEntry("error", "desktop_ui", message));
  } catch {
    // store not ready yet — the durable log already has it
  }
});

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <React.StrictMode>
    <AppConfigProvider>
      <DiagnosticErrorBoundary>
        <PluginShell />
        <StartupReveal />
      </DiagnosticErrorBoundary>
    </AppConfigProvider>
  </React.StrictMode>,
);
