import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import { AppConfigProvider, useAppConfig } from "./config";
import { showMainWindow } from "./lib/windowLifecycle";
import { makeUiLogEntry, writeLogToConsole } from "./lib/logger";
import { PluginProvider } from "./plugins/PluginProvider";
import { useCadCoreStore } from "./state";
import "./i18n";
import "./styles.css";

// [DIAGNOSTIC — temporary] Catches render-loop errors and logs the
// COMPONENT STACK — the exact render path that recursed.  Removed
// once the loop is fixed.
class DiagnosticErrorBoundary extends React.Component<
  { children: React.ReactNode },
  { failed: boolean }
> {
  constructor(props: { children: React.ReactNode }) {
    super(props);
    this.state = { failed: false };
  }

  static getDerivedStateFromError() {
    return { failed: true };
  }

  componentDidCatch(error: unknown) {
    const componentStack =
      error instanceof Error
        ? (error as Error & { componentStack?: string }).componentStack ?? ""
        : "";
    const entry = makeUiLogEntry(
      "error",
      "desktop_ui",
      `render error: ${String(error).slice(0, 2000)}\ncomponent stack:\n${
        componentStack.slice(0, 6000) || "(none)"
      }`,
    );
    writeLogToConsole(entry);
    useCadCoreStore.getState().addLogEntry(entry);
  }

  render() {
    if (this.state.failed) {
      return <div />;
    }
    return this.props.children;
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
