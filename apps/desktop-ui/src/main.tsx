import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import { AppConfigProvider, useAppConfig } from "./config";
import { showMainWindow } from "./lib/windowLifecycle";
import { PluginProvider } from "./plugins/PluginProvider";
import { useCadCoreStore } from "./state";
import "./i18n";
import "./styles.css";

const startupStartedAt = performance.now();
const minimumSplashDurationMs = 350;

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
      <PluginShell />
      <StartupReveal />
    </AppConfigProvider>
  </React.StrictMode>,
);
