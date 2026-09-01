import type { RefObject } from "react";
import { openUrl } from "@tauri-apps/plugin-opener";
import type { AppConfig } from "../lib";

interface SlicerWorkspaceProps {
  orcaSlicer: AppConfig["orcaSlicer"];
  slicerViewportRef: RefObject<HTMLDivElement | null>;
  hasOrcaEmbedSession: boolean;
  slicerStatus: string | null;
  waitingMessage: string;
  externalMessage: string;
  openInBrowserLabel: string;
  addMessage: (message: string) => void;
}

export function SlicerWorkspace({
  orcaSlicer,
  slicerViewportRef,
  hasOrcaEmbedSession,
  slicerStatus,
  waitingMessage,
  externalMessage,
  openInBrowserLabel,
  addMessage,
}: SlicerWorkspaceProps) {
  if (orcaSlicer.enabled && orcaSlicer.integrationMode === "web") {
    return (
      <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
        <div className="flex items-center gap-2 border-b border-white/10 px-3 py-1.5">
          <span className="text-xs text-on-surface-muted">OrcaSlicer Web</span>
          <div className="ml-auto">
            <button
              type="button"
              className="rounded-md px-2 py-0.5 text-xs text-on-surface-muted transition-colors hover:bg-white/10 hover:text-on-surface"
              onClick={() => {
                void openUrl(orcaSlicer.webUrl).catch((error) => {
                  addMessage(
                    `slicer: failed to open browser: ${String(error)}`,
                  );
                });
              }}
            >
              {openInBrowserLabel}
            </button>
          </div>
        </div>
        <iframe
          src={orcaSlicer.webUrl}
          className="flex-1 w-full border-0"
          title="OrcaSlicer"
          allow="autoplay;camera;microphone;fullscreen"
        />
      </section>
    );
  }

  if (orcaSlicer.enabled && orcaSlicer.integrationMode === "external") {
    // OrcaSlicer runs in its own window; this view only explains the mode
    // and surfaces the status of the last export/launch.
    return (
      <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
        <div className="flex min-h-0 w-full min-w-0 flex-1 items-center justify-center bg-surface-lowest">
          <span className="max-w-xl px-6 text-center text-sm text-on-surface-muted">
            {slicerStatus ?? externalMessage}
          </span>
        </div>
      </section>
    );
  }

  return (
    <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
      <div
        id="slicer-viewport-container"
        ref={slicerViewportRef}
        className="flex min-h-0 w-full min-w-0 flex-1 items-center justify-center bg-surface-lowest"
      >
        {hasOrcaEmbedSession ? null : (
          <span className="max-w-xl px-6 text-center text-sm text-on-surface-muted">
            {slicerStatus ?? waitingMessage}
          </span>
        )}
      </div>
    </section>
  );
}
