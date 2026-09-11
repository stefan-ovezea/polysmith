import { useEffect, useMemo, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import { useTranslation } from "react-i18next";

import { CamGrblPanel } from "../layout";
import { useGrblStore, useToastStore } from "../state";
import {
  grblParseFile,
  type GcodeFileInfo,
} from "@/lib/grblClient";
import type { LaserMachineSettings } from "@/types";
import { GrblPreviewViewport } from "./grbl/GrblPreviewViewport";
import { DEFAULT_GRBL_BED, type GrblBed } from "./grbl/grblPreviewScene";

interface GrblWorkspaceProps {
  // File selected by the CAM "Send to GRBL workspace" handoff (P4), or
  // null when the user arrived via the workspace switcher.
  embeddedFilePath: string | null;
  machineSettings: LaserMachineSettings | null;
  theme: string;
  addMessage: (message: string) => void;
  // Handoff wiring (P4): set together — the workspace parses
  // `previewFile` on arrival and reports back that it was consumed.
  previewFile?: string | null;
  onPreviewFileConsumed?: () => void;
}

interface LoadedGrblFile {
  path: string;
  info: GcodeFileInfo;
}

// Standalone GRBL workspace page: fixed left column hosting the same
// GRBL machine panel the CAM setup entry uses (both coexist — the panel
// is rendered embedded here, floating there), right side showing the
// parsed toolpath with live progress and machine position.  No sidebar,
// no timeline, no CAM panels — the page template follows
// SlicerWorkspace.
export function GrblWorkspace({
  embeddedFilePath,
  machineSettings,
  theme,
  addMessage,
  previewFile,
  onPreviewFileConsumed,
}: GrblWorkspaceProps) {
  const { t } = useTranslation();

  const completed = useGrblStore((state) => state.completed);
  const streaming = useGrblStore((state) => state.streaming);
  const linesSent = useGrblStore((state) => state.linesSent);
  const mpos = useGrblStore((state) => state.mpos);
  const wpos = useGrblStore((state) => state.wpos);

  const [loadedFile, setLoadedFile] = useState<LoadedGrblFile | null>(null);
  const [loading, setLoading] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);

  const bed = useMemo<GrblBed>(() => {
    if (
      machineSettings &&
      machineSettings.work_area_x_mm > 0 &&
      machineSettings.work_area_y_mm > 0
    ) {
      return {
        widthMm: machineSettings.work_area_x_mm,
        heightMm: machineSettings.work_area_y_mm,
      };
    }
    return DEFAULT_GRBL_BED;
  }, [machineSettings]);

  const loadFile = async (path: string) => {
    setLoading(true);
    setLoadError(null);
    try {
      const info = await grblParseFile(path);
      setLoadedFile({ path, info });
    } catch (error) {
      const message = String(error);
      setLoadError(message);
      addMessage(`grbl preview: ${message}`);
      useToastStore.getState().pushToast("error", t("grbl.loadError"));
    } finally {
      setLoading(false);
    }
  };

  const openFile = async () => {
    try {
      const selected = await open({
        multiple: false,
        directory: false,
        filters: [
          {
            name: t("grbl.gcodeFiles"),
            extensions: ["nc", "gcode", "tap", "ngc", "gc", "txt"],
          },
        ],
      });
      if (typeof selected === "string") {
        await loadFile(selected);
      }
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  // CAM handoff: parse the file the CAM workspace exported for us.
  useEffect(() => {
    if (previewFile) {
      void loadFile(previewFile);
      onPreviewFileConsumed?.();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [previewFile]);

  // Executed-line mapping: `linesSent` counts buffered-not-executed
  // lines (≤127-byte lead) — the same approximation LaserGRBL shows.
  // On completion every line is highlighted; reset clears to zero.
  const executedLines = useMemo(() => {
    if (!loadedFile) {
      return 0;
    }
    if (completed) {
      const moves = loadedFile.info.moves;
      return moves.length > 0 ? moves[moves.length - 1].line : 0;
    }
    return streaming ? linesSent : 0;
  }, [loadedFile, completed, streaming, linesSent]);

  // WCS offset (WCO = MPos − WPos): shifts the previewed toolpath
  // from file space into machine space, so the MPos crosshair
  // overlays it and "Zero XY" moves the job onto the head.
  const wco = useMemo<[number, number, number] | null>(() => {
    if (mpos && wpos) {
      return [
        mpos[0] - wpos[0],
        mpos[1] - wpos[1],
        mpos[2] - wpos[2],
      ];
    }
    return null;
  }, [mpos, wpos]);

  const legend = [
    { label: t("grbl.legendRapid"), token: "--cad-toolpath-rapid" },
    { label: t("grbl.legendCut"), token: "--cad-toolpath-feed" },
    { label: t("grbl.legendExecuted"), token: "--cad-toolpath-executed" },
    { label: t("grbl.legendPosition"), token: "--color-primary-edge-active" },
  ];

  return (
    <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
      <div className="flex items-center gap-3 border-b border-[var(--cad-panel-soft-border)] px-3 py-1.5">
        <span className="cad-kicker">{t("workspace.grbl")}</span>
        <button
          type="button"
          className="cad-action-ghost px-2 py-0.5 text-[10px] uppercase tracking-wider"
          onClick={() => {
            void openFile();
          }}
        >
          {t("grbl.openFile")}
        </button>
        {loadedFile ? (
          <span className="min-w-0 truncate font-mono text-[10px] text-on-surface-dim">
            {loadedFile.info.fileName}
            {" · "}
            {t("grbl.moves", { count: loadedFile.info.moves.length })}
          </span>
        ) : null}
      </div>
      <div className="flex min-h-0 w-full min-w-0 flex-1">
        <aside className="min-h-0 w-[340px] shrink-0 border-r border-[var(--cad-panel-soft-border)] bg-surface-lowest">
          <CamGrblPanel
            embedded
            // The previewed file becomes the program Cycle Start sends;
            // the CAM handoff path (P4) fills in when no file is open.
            embeddedFilePath={loadedFile?.path ?? embeddedFilePath}
            // A file picked through the panel's Load button must parse
            // and preview here too — same pipeline as the header Open
            // button, otherwise Cycle Start would stream a program the
            // viewport never shows.
            onFileLoaded={(path) => {
              void loadFile(path);
            }}
            onClose={() => {}}
          />
        </aside>
        <div className="relative flex min-h-0 min-w-0 flex-1 bg-surface-lowest">
          <GrblPreviewViewport
            info={loadedFile?.info ?? null}
            bed={bed}
            executedLines={executedLines}
            // Machine-space overlay: the crosshair is the raw machine
            // position; the toolpath is shifted by the WCS offset.
            mpos={mpos}
            wco={wco}
            theme={theme}
          />

          {loading ? (
            <div className="pointer-events-none absolute inset-0 flex items-center justify-center">
              <span className="text-sm text-on-surface-muted">
                {t("grbl.loading")}
              </span>
            </div>
          ) : null}

          {!loadedFile && !loading ? (
            <div className="pointer-events-none absolute inset-0 flex items-center justify-center">
              <span className="max-w-xl px-6 text-center text-sm text-on-surface-muted">
                {loadError ? t("grbl.loadError") : t("grbl.noFile")}
              </span>
            </div>
          ) : null}

          {loadError && !loading ? (
            <div className="pointer-events-none absolute inset-x-0 bottom-10 flex justify-center">
              <span className="max-w-lg truncate px-4 font-mono text-[10px] text-danger">
                {loadError}
              </span>
            </div>
          ) : null}

          {loadedFile && !loading ? (
            <div className="pointer-events-none absolute bottom-3 left-3 flex flex-wrap items-center gap-3 rounded-md bg-[var(--cad-panel-soft-bg)] px-2.5 py-1.5">
              {legend.map((entry) => (
                <span
                  key={entry.token}
                  className="flex items-center gap-1.5 text-[10px] uppercase tracking-wider text-on-surface-dim"
                >
                  <span
                    className="inline-block h-2 w-2 rounded-full"
                    style={{ background: `var(${entry.token})` }}
                  />
                  {entry.label}
                </span>
              ))}
            </div>
          ) : null}

          {loadedFile && !loading && loadedFile.info.warnings.length > 0 ? (
            <div className="absolute bottom-3 right-3 max-w-[45%] rounded-md bg-[var(--cad-panel-soft-bg)] px-2.5 py-1.5">
              <p className="text-[10px] uppercase tracking-wider text-on-surface-muted">
                {t("grbl.parseWarnings")}
              </p>
              <ul className="mt-1 max-h-24 space-y-0.5 overflow-y-auto font-mono text-[10px] text-on-surface-dim">
                {loadedFile.info.warnings.map((warning) => (
                  <li key={warning}>{warning}</li>
                ))}
              </ul>
            </div>
          ) : null}
        </div>
      </div>
    </section>
  );
}
