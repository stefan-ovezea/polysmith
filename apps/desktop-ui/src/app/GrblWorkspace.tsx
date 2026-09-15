import { useEffect, useMemo, useRef, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import { useTranslation } from "react-i18next";

import { useCadCore } from "../hooks";
import { CamGrblPanel } from "../layout";
import { useGrblStore, useToastStore } from "../state";
import { Dropdown } from "@/lib";
import {
  grblParseFile,
  grblParseText,
  grblReset,
  grblSendProgram,
  grblSendRaw,
  grblUtilityProgram,
  type GcodeFileInfo,
  type GrblUtilityRequest,
} from "@/lib/grblClient";
import type { LaserMachineSettings, MachineDefinition } from "@/types";
import { GrblPreviewViewport } from "./grbl/GrblPreviewViewport";
import { GrblUtilitiesPanel } from "./grbl/GrblUtilitiesPanel";
import { DEFAULT_GRBL_BED, type GrblBed } from "./grbl/grblPreviewScene";

// A program handed over by the CAM workspace — posted G-code text in
// memory, no file on disk.
export interface GrblHandoffProgram {
  text: string;
  label: string;
}

// localStorage key pattern matches the host/port persistence in
// CamGrblPanel ("polysmith.grbl.*").
const MACHINE_STORAGE_KEY = "polysmith.grbl.machineName";

interface GrblWorkspaceProps {
  // Program selected by the CAM "Send to GRBL workspace" handoff, or
  // null when the user arrived via the workspace switcher.
  embeddedProgram: GrblHandoffProgram | null;
  machineSettings: LaserMachineSettings | null;
  theme: string;
  addMessage: (message: string) => void;
  // Handoff wiring: set together — the workspace parses the in-memory
  // program on arrival and reports back that it was consumed.
  handoffProgram?: GrblHandoffProgram | null;
  onHandoffConsumed?: () => void;
}

interface LoadedGrblProgram {
  // "file" = a disk pick (streamed from path); "internal" = in-memory
  // posted text (CAM handoff, streamed via grbl_send_program).
  source: "file" | "internal";
  text: string;
  label: string;
  info: GcodeFileInfo;
}

// Standalone GRBL workspace page: fixed left column hosting the same
// GRBL machine panel the CAM setup entry uses (both coexist — the panel
// is rendered embedded here, floating there) plus the laser utilities
// section, right side showing the parsed toolpath with live progress,
// machine position, red pointer dot, and utility overlay.  No sidebar,
// no timeline, no CAM panels — the page template follows
// SlicerWorkspace.
export function GrblWorkspace({
  embeddedProgram,
  machineSettings,
  theme,
  addMessage,
  handoffProgram,
  onHandoffConsumed,
}: GrblWorkspaceProps) {
  const { t } = useTranslation();
  const { camMachineList } = useCadCore();

  const connected = useGrblStore((state) => state.connected);
  const completed = useGrblStore((state) => state.completed);
  const streaming = useGrblStore((state) => state.streaming);
  const linesSent = useGrblStore((state) => state.linesSent);
  const mpos = useGrblStore((state) => state.mpos);
  const wpos = useGrblStore((state) => state.wpos);

  const [loadedProgram, setLoadedProgram] =
    useState<LoadedGrblProgram | null>(null);
  // Disk path of the loaded program — flows down to the panel so the
  // toolbar Open button also enables Cycle Start there.
  const [loadedFilePath, setLoadedFilePath] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);

  // Machine library (P2): definitions are JSON files on disk, listed
  // by the core.  The selection is a workspace-local override (like
  // the host/port fields) — it does not write back to the document.
  const [machines, setMachines] = useState<MachineDefinition[]>([]);
  const [selectedMachineName, setSelectedMachineName] = useState<
    string | null
  >(() => {
    try {
      return localStorage.getItem(MACHINE_STORAGE_KEY);
    } catch {
      return null;
    }
  });
  // The list fetch is retried with backoff: the workspace can mount
  // before the core process has been registered by the shell (app
  // launch restoring the GRBL page), and a one-shot fetch fails
  // permanently with "cad_core is not running" — an empty machine
  // dropdown that never recovers.  Up to 4 attempts over ~6 s.
  const machineFetchAttemptsRef = useRef(0);

  useEffect(() => {
    let cancelled = false;
    let retryTimer: number | undefined;
    const attempt = () => {
      camMachineList()
        .then((list) => {
          if (!cancelled) {
            setMachines(list);
          }
        })
        .catch((error) => {
          if (cancelled) {
            return;
          }
          machineFetchAttemptsRef.current += 1;
          const attemptNumber = machineFetchAttemptsRef.current;
          if (attemptNumber < 4) {
            retryTimer = window.setTimeout(
              attempt,
              800 * attemptNumber,
            );
            return;
          }
          addMessage(`grbl machines: ${String(error)}`);
          useToastStore
            .getState()
            .pushToast("error", t("cam.setup.machineListFailed"));
        });
    };
    attempt();
    return () => {
      cancelled = true;
      if (retryTimer !== undefined) {
        window.clearTimeout(retryTimer);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const selectedMachine = useMemo(
    () => machines.find((machine) => machine.name === selectedMachineName) ?? null,
    [machines, selectedMachineName],
  );

  const handleMachineChange = (name: string) => {
    setSelectedMachineName(name);
    try {
      localStorage.setItem(MACHINE_STORAGE_KEY, name);
    } catch {
      // localStorage can be unavailable in odd webview contexts —
      // non-fatal, the selection just won't persist.
    }
  };

  // Bed fallback order: selected machine → document machine settings
  // → the LaserGRBL-style default seed.
  const bed = useMemo<GrblBed>(() => {
    if (
      selectedMachine &&
      selectedMachine.work_area_x_mm > 0 &&
      selectedMachine.work_area_y_mm > 0
    ) {
      return {
        widthMm: selectedMachine.work_area_x_mm,
        heightMm: selectedMachine.work_area_y_mm,
      };
    }
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
  }, [selectedMachine, machineSettings]);

  // Red pointer offset (P3): selected machine wins, the document's
  // machine settings follow, [0,0] otherwise.
  const pointerOffset = useMemo<[number, number]>(() => {
    if (selectedMachine) {
      return [
        selectedMachine.pointer_offset_x_mm,
        selectedMachine.pointer_offset_y_mm,
      ];
    }
    if (machineSettings) {
      return [
        machineSettings.pointer_offset_x_mm,
        machineSettings.pointer_offset_y_mm,
      ];
    }
    return [0, 0];
  }, [selectedMachine, machineSettings]);

  // Red pointer state (P3): the workspace owns it because the viewport
  // renders the dot; the utilities panel hosts the toggle.
  const [pointerOn, setPointerOn] = useState(false);

  // Pointer laser power from the selected machine (percentage of
  // GRBL's 255-unit laser scale).  5 % ≈ S13, the previous hardcoded
  // value; clamped so a 0 % machine still fires visibly at minimum.
  const pointerPowerS = useMemo(() => {
    const percent = selectedMachine?.pointer_power_percent ?? 5;
    return Math.min(255, Math.max(1, Math.round((255 * percent) / 100)));
  }, [selectedMachine]);

  // GRBL prefs from the selected machine (jog presets + homing).
  // Null when no machine is selected — the panel keeps its editable
  // defaults in that case (same as the standalone CAM panel).
  const grblMachinePrefs = useMemo(() => {
    if (!selectedMachine) {
      return null;
    }
    return {
      jogStepMm: selectedMachine.jog_step_mm,
      jogFeedMmPerMin: selectedMachine.jog_feed_mm_per_min,
      homingEnabled: selectedMachine.homing_enabled,
    };
  }, [selectedMachine]);

  // Disconnecting drops the beam — never leave the toggle claiming it
  // is still on after a reconnect.
  useEffect(() => {
    if (!connected) {
      setPointerOn(false);
    }
  }, [connected]);

  const togglePointer = async () => {
    const next = !pointerOn;
    try {
      await grblSendRaw(next ? `M3 S${pointerPowerS}` : "M5");
      setPointerOn(next);
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  // Utility overlay (P4): the framing box / focus pulse program is
  // parsed like any job, drawn in the overlay color, and streamed with
  // its own progress count.  `sendingTarget` tracks which program the
  // stream counters belong to.
  const [overlayProgram, setOverlayProgram] = useState<{
    info: GcodeFileInfo;
    text: string;
    label: string;
  } | null>(null);
  const [sendingTarget, setSendingTarget] = useState<"main" | "utility">(
    "main",
  );

  const runUtility = async (request: GrblUtilityRequest) => {
    if (!connected) {
      return;
    }
    try {
      const { text, label } = await grblUtilityProgram(request);
      const info = await grblParseText(text, label);
      setOverlayProgram({ info, text, label });
      setSendingTarget("utility");
      await grblSendProgram(text);
    } catch (error) {
      addMessage(`grbl utility: ${String(error)}`);
      useToastStore.getState().pushToast("error", t("grbl.util.runFailed"));
    }
  };

  const clearOverlay = () => {
    setOverlayProgram(null);
    setSendingTarget("main");
  };

  // Big STOP: hard-abort whatever the machine is doing right now.  The
  // soft reset (0x18) stops motion and drops the laser; the utility
  // overlay is cleared so the preview matches the machine state.
  const handleStop = () => {
    void grblReset().catch((error) => {
      addMessage(`grbl stop: ${String(error)}`);
      useToastStore.getState().pushToast("error", t("grbl.stopError"));
    });
    clearOverlay();
  };

  // A new main program replaces any utility overlay.
  const loadFile = async (path: string) => {
    setLoading(true);
    setLoadError(null);
    try {
      const info = await grblParseFile(path);
      setLoadedProgram({ source: "file", text: "", label: info.fileName, info });
      setLoadedFilePath(path);
      clearOverlay();
    } catch (error) {
      const message = String(error);
      setLoadError(message);
      addMessage(`grbl preview: ${message}`);
      useToastStore.getState().pushToast("error", t("grbl.loadError"));
    } finally {
      setLoading(false);
    }
  };

  // Internal (CAM handoff) path: parse the posted text in memory —
  // the same program Cycle Start streams, never touching disk.
  const loadProgram = async (text: string, label: string) => {
    setLoading(true);
    setLoadError(null);
    try {
      const info = await grblParseText(text, label);
      setLoadedProgram({ source: "internal", text, label, info });
      setLoadedFilePath(null);
      clearOverlay();
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

  // CAM handoff: parse the program the CAM workspace posted for us.
  useEffect(() => {
    if (handoffProgram) {
      void loadProgram(handoffProgram.text, handoffProgram.label);
      onHandoffConsumed?.();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [handoffProgram]);

  // Executed-line mapping: `linesSent` counts buffered-not-executed
  // lines (≤127-byte lead) — the same approximation LaserGRBL shows.
  // On completion every line is highlighted; reset clears to zero.
  const executedLines = useMemo(() => {
    if (!loadedProgram) {
      return 0;
    }
    if (completed) {
      const moves = loadedProgram.info.moves;
      return moves.length > 0 ? moves[moves.length - 1].line : 0;
    }
    return streaming ? linesSent : 0;
  }, [loadedProgram, completed, streaming, linesSent]);

  // While a utility streams, the counters belong to the overlay — the
  // main toolpath highlight freezes at zero so a framing run never
  // mis-highlights the job.
  const mainExecutedLines = sendingTarget === "utility" ? 0 : executedLines;

  const overlayExecutedLines = useMemo(() => {
    if (!overlayProgram || sendingTarget !== "utility") {
      return 0;
    }
    if (completed) {
      const moves = overlayProgram.info.moves;
      return moves.length > 0 ? moves[moves.length - 1].line : 0;
    }
    return streaming ? linesSent : 0;
  }, [overlayProgram, sendingTarget, completed, streaming, linesSent]);

  // The program handed down to the machine panel.  Memoized: the
  // panel syncs its internal copy in an identity-keyed effect, and a
  // fresh object every render made that effect setState each pass —
  // "Maximum update depth exceeded" (hot over TCP status events).
  const panelProgram = useMemo(() => {
    if (loadedProgram?.source === "internal") {
      return { text: loadedProgram.text, label: loadedProgram.label };
    }
    return embeddedProgram ?? null;
  }, [loadedProgram, embeddedProgram]);

  // Bed check (P6): the parsed bounds are WCS coordinates (origin =
  // the job's own 0,0), so anything below 0 or beyond the work area
  // would trip the soft limits once cut.  Warning only — the user may
  // frame the job anywhere.
  const jobExceedsBed = useMemo(() => {
    const bounds = loadedProgram?.info.bounds ?? null;
    if (!bounds) {
      return false;
    }
    return (
      bounds.minX < 0 ||
      bounds.minY < 0 ||
      bounds.maxX > bed.widthMm ||
      bounds.maxY > bed.heightMm
    );
  }, [loadedProgram, bed]);

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
    { label: t("grbl.legendExecuted"), token: "--cad-toolpath-burned" },
    { label: t("grbl.legendPosition"), token: "--color-primary-edge-active" },
    { label: t("grbl.legendPointer"), token: "--cad-pointer-dot" },
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
        <label className="flex items-center gap-2 text-[10px] uppercase tracking-wider text-on-surface-muted">
          {t("grbl.machineLabel")}
          <Dropdown
            className="w-44"
            value={selectedMachineName ?? "__none__"}
            label={t("grbl.machineLabel")}
            options={[
              { value: "__none__", label: t("grbl.machineNone") },
              ...machines.map((machine) => ({
                value: machine.name,
                label: machine.name,
              })),
            ]}
            onChange={(value) =>
              handleMachineChange(value === "__none__" ? "" : value)
            }
          />
        </label>
        {/* The picked machine's facts — bed, pointer offset, homing —
            all come from the machine definition (CAM → Setup →
            Machine), so the values behind the preview are visible
            right here. */}
        {selectedMachine ? (
          <span className="font-mono text-[9px] text-on-surface-dim">
            {t("grbl.machineFacts", {
              width: selectedMachine.work_area_x_mm,
              height: selectedMachine.work_area_y_mm,
              offsetX: selectedMachine.pointer_offset_x_mm,
              offsetY: selectedMachine.pointer_offset_y_mm,
              homing: selectedMachine.homing_enabled
                ? t("grbl.homingYes")
                : t("grbl.homingNo"),
              power: selectedMachine.pointer_power_percent,
            })}
          </span>
        ) : (
          <span className="text-[9px] leading-snug text-on-surface-dim">
            {t("grbl.machineNoneHint")}
          </span>
        )}
        {loadedProgram ? (
          <span className="min-w-0 truncate font-mono text-[10px] text-on-surface-dim">
            {loadedProgram.info.fileName}
            {" · "}
            {t("grbl.moves", { count: loadedProgram.info.moves.length })}
          </span>
        ) : null}
        {/* The one button that must never be subtle: kills the job
            instantly, same 0x18 path as the panel Reset but styled
            like the emergency it is. */}
        <button
          type="button"
          className="ml-auto rounded-md bg-danger px-4 py-1.5 text-sm font-bold uppercase tracking-wider text-on-primary hover:bg-danger/90 disabled:opacity-40"
          disabled={!connected}
          onClick={handleStop}
          title={t("grbl.stopTitle")}
        >
          {t("grbl.stop")}
        </button>
      </div>
      <div className="flex min-h-0 w-full min-w-0 flex-1">
        <aside className="flex min-h-0 w-[340px] shrink-0 flex-col border-r border-[var(--cad-panel-soft-border)] bg-surface-lowest">
          <div className="min-h-0 flex-1">
            <CamGrblPanel
              embedded
              // Jog presets + Home availability from the picked machine
              // (null = no machine → editable defaults, Home visible).
              machinePrefs={grblMachinePrefs}
              // Per-machine $$ snapshot persistence (Restore in the
              // settings dialog).
              settingsMachineName={selectedMachineName}
              // Toolbar Open loads a disk file here — hand the path
              // down so the panel's Cycle Start streams it too.
              externalPath={loadedFilePath}
              // The previewed program becomes what Cycle Start sends; the
              // CAM handoff program fills in when nothing is loaded yet.
              embeddedProgram={panelProgram}
              // A file picked through the panel's Load button must parse
              // and preview here too — same pipeline as the header Open
              // button, otherwise Cycle Start would stream a program the
              // viewport never shows.
              onFileLoaded={(path) => {
                void loadFile(path);
              }}
              // Cycle Start streams the MAIN program — drop the utility
              // overlay so the highlight tracks the job again.
              onCycleStart={() => {
                clearOverlay();
              }}
              onClose={() => {}}
            />
          </div>
          <GrblUtilitiesPanel
            connected={connected}
            pointerOn={pointerOn}
            onTogglePointer={() => {
              void togglePointer();
            }}
            jobBounds={loadedProgram?.info.bounds ?? null}
            onRunUtility={(request) => {
              void runUtility(request);
            }}
            hasOverlay={overlayProgram !== null}
            onClearOverlay={clearOverlay}
          />
        </aside>
        <div className="relative flex min-h-0 min-w-0 flex-1 bg-surface-lowest">
          <GrblPreviewViewport
            info={loadedProgram?.info ?? null}
            bed={bed}
            executedLines={mainExecutedLines}
            // Machine-space overlay: the crosshair is the raw machine
            // position; the toolpath is shifted by the WCS offset.
            mpos={mpos}
            wco={wco}
            pointerOffset={pointerOffset}
            pointerOn={pointerOn}
            overlayInfo={overlayProgram?.info ?? null}
            overlayExecutedLines={overlayExecutedLines}
            theme={theme}
          />

          {loading ? (
            <div className="pointer-events-none absolute inset-0 flex items-center justify-center">
              <span className="text-sm text-on-surface-muted">
                {t("grbl.loading")}
              </span>
            </div>
          ) : null}

          {loadedProgram && !loading && jobExceedsBed ? (
            <div className="pointer-events-none absolute inset-x-0 top-3 flex justify-center">
              <span className="rounded-md border border-[var(--cad-panel-soft-border)] bg-[var(--cad-panel-soft-bg)] px-3 py-1.5 font-mono text-[10px] text-on-surface">
                {t("grbl.jobExceedsBed", {
                  width: bed.widthMm,
                  height: bed.heightMm,
                })}
              </span>
            </div>
          ) : null}

          {!loadedProgram && !loading ? (
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

          {loadedProgram && !loading ? (
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

          {loadedProgram && !loading && loadedProgram.info.warnings.length > 0 ? (
            <div className="absolute bottom-3 right-3 max-w-[45%] rounded-md bg-[var(--cad-panel-soft-bg)] px-2.5 py-1.5">
              <p className="text-[10px] uppercase tracking-wider text-on-surface-muted">
                {t("grbl.parseWarnings")}
              </p>
              <ul className="mt-1 max-h-24 space-y-0.5 overflow-y-auto font-mono text-[10px] text-on-surface-dim">
                {loadedProgram.info.warnings.map((warning) => (
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
