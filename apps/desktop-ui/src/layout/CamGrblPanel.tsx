import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { open } from "@tauri-apps/plugin-dialog";

import { Dropdown } from "@/lib";
import {
  grblConnect,
  grblConnectTcp,
  grblDisconnect,
  grblGetSettings,
  grblHome,
  grblJog,
  grblPause,
  grblReset,
  grblResume,
  grblSendFile,
  grblSendProgram,
  grblSendRaw,
  grblWriteByte,
  grblUnlock,
  grblZeroXy,
  listGrblPorts,
  type GrblPortInfo,
} from "@/lib/grblClient";
import { initGrblStreamListener, useGrblStore, useToastStore } from "@/state";
import { GrblSettingsDialog } from "./GrblSettingsDialog";
import { useCamEscapeCancel } from "./camPanelShared";

const BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400];

// Stable no-op for the embedded Escape handler (see below).
const NOOP = () => {};

// Direct GRBL transport panel.  The shell (gcode_sender.rs) owns the
// serial port; this panel only issues commands and renders the
// `grbl-stream` events from useGrblStore.  No document interaction —
// it streams .nc files picked from disk, or in-memory programs posted
// by the CAM workspace, exactly like LaserGRBL.
//
// `embedded` turns this into the GRBL workspace's fixed left column:
// no Close button, no Escape-to-cancel, and a full-height plain panel
// instead of the floating card. `embeddedProgram` (when set) makes
// Cycle Start stream that in-memory program directly instead of
// opening a picker — used by the CAM handoff. `onFileLoaded` lets the
// host (the workspace) learn about a disk-picked file so its preview
// parses the same program the panel will stream. The CAM setup entry
// passes neither optional prop.
export interface GrblPanelProgram {
  text: string;
  label: string;
}

export function CamGrblPanel({
  onClose,
  embedded,
  embeddedProgram,
  onFileLoaded,
  onCycleStart,
}: {
  onClose: () => void;
  embedded?: boolean;
  embeddedProgram?: GrblPanelProgram | null;
  onFileLoaded?: (path: string) => void;
  // Notified right before a job is streamed — the GRBL workspace uses
  // it to drop the laser-utility overlay so the main program's
  // progress highlights correctly again.
  onCycleStart?: () => void;
}) {
  const { t } = useTranslation();

  const connected = useGrblStore((state) => state.connected);
  const streaming = useGrblStore((state) => state.streaming);
  const paused = useGrblStore((state) => state.paused);
  const linesSent = useGrblStore((state) => state.linesSent);
  const linesTotal = useGrblStore((state) => state.linesTotal);
  const percent = useGrblStore((state) => state.percent);
  const machineState = useGrblStore((state) => state.machineState);
  const mpos = useGrblStore((state) => state.mpos);
  const wpos = useGrblStore((state) => state.wpos);
  const lastMessage = useGrblStore((state) => state.lastMessage);

  const [ports, setPorts] = useState<GrblPortInfo[]>([]);
  const [selectedPort, setSelectedPort] = useState<string | null>(null);
  const [baudRate, setBaudRate] = useState("115200");
  // Transport: USB serial (COM port + baud) or FluidNC's TCP text
  // port (host + port 23) — both speak the same line protocol.
  const [connectMode, setConnectMode] = useState<"serial" | "tcp">("serial");
  // Remember the last TCP target so reconnecting is one click.
  const [host, setHost] = useState(() => {
    try {
      return localStorage.getItem("polysmith.grbl.host") ?? "";
    } catch {
      return "";
    }
  });
  const [tcpPort, setTcpPort] = useState(() => {
    try {
      return localStorage.getItem("polysmith.grbl.tcpPort") ?? "23";
    } catch {
      return "23";
    }
  });
  const [jogStep, setJogStep] = useState(10);
  const [jogFeed, setJogFeed] = useState(1000);
  const [busy, setBusy] = useState(false);
  // Live overrides (P5): slider targets; committed to the controller
  // as real-time bytes on release.
  const [feedOverride, setFeedOverride] = useState(100);
  const [powerOverride, setPowerOverride] = useState(100);
  // Job timer: ticks while streaming, freezes on completion.
  const [elapsedSeconds, setElapsedSeconds] = useState(0);
  // The program loaded for streaming.  Loading NEVER sends — Cycle
  // Start does.  Two sources: a disk file (loadedPath, streamed via
  // grbl_send_file) or in-memory posted text (loadedProgram, the CAM
  // handoff, streamed via grbl_send_program).  In the GRBL workspace
  // the host passes the previewed program down; in the CAM panel Load
  // picks a file.
  const [loadedPath, setLoadedPath] = useState<string | null>(null);
  const [loadedProgram, setLoadedProgram] = useState<GrblPanelProgram | null>(
    null,
  );
  const [rawCommand, setRawCommand] = useState("");
  // Console history (P7): up-arrow recalls, down-arrow steps forward.
  const [commandHistory, setCommandHistory] = useState<string[]>([]);
  const [historyIndex, setHistoryIndex] = useState<number | null>(null);
  // `$$` settings dialog state.
  const [settingsOpen, setSettingsOpen] = useState(false);
  const settings = useGrblStore((state) => state.settings);

  const formatPosition = (position: [number, number, number]) =>
    `X ${position[0].toFixed(2)}  Y ${position[1].toFixed(2)}  Z ${position[2].toFixed(2)}`;
  // WCS is the job coordinate system the .nc uses; MCS is the raw
  // machine position. They match until "Zero XY" shifts G54.
  const wcsPosition = wpos ?? mpos;

  useCamEscapeCancel(embedded ? NOOP : onClose);

  const refreshPorts = async () => {
    try {
      const found = await listGrblPorts();
      setPorts(found);
      setSelectedPort((current) => {
        if (current && found.some((port) => port.name === current)) {
          return current;
        }
        return found[0]?.name ?? null;
      });
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  useEffect(() => {
    initGrblStreamListener();
    void refreshPorts();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // The workspace/handoff program is what Cycle Start sends; keep it
  // in sync so the button always streams what the preview shows.
  useEffect(() => {
    if (embeddedProgram) {
      setLoadedProgram(embeddedProgram);
    }
  }, [embeddedProgram]);

  const runCommand = async (action: () => Promise<void>) => {
    setBusy(true);
    try {
      await action();
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    } finally {
      setBusy(false);
    }
  };

  const connect = async () => {
    if (connectMode === "tcp") {
      const trimmedHost = host.trim();
      if (!trimmedHost) {
        return;
      }
      const portNumber = Number(tcpPort) || 23;
      try {
        localStorage.setItem("polysmith.grbl.host", trimmedHost);
        localStorage.setItem("polysmith.grbl.tcpPort", String(portNumber));
      } catch {
        // localStorage can be unavailable in odd webview contexts —
        // non-fatal, the connection still proceeds.
      }
      await runCommand(() => grblConnectTcp(trimmedHost, portNumber));
      return;
    }
    if (!selectedPort) {
      return;
    }
    await runCommand(() => grblConnect(selectedPort, Number(baudRate)));
  };

  // Load only picks and remembers the program — the machine is
  // untouched until Cycle Start (the laser workflow needs the file
  // previewed and the origin zeroed between the two). The host gets
  // the pick so the workspace preview shows the same program.
  const loadFile = async () => {
    try {
      const selected = await open({
        multiple: false,
        directory: false,
        filters: [
          {
            name: t("cam.grbl.gcodeFiles", "G-code files"),
            extensions: ["nc", "gcode", "tap", "ngc", "gc", "txt"],
          },
        ],
      });
      if (typeof selected === "string") {
        // A disk pick replaces any in-memory program — the preview
        // and Cycle Start must agree on the single loaded program.
        setLoadedPath(selected);
        setLoadedProgram(null);
        onFileLoaded?.(selected);
      }
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  const showProgress = streaming || (connected && linesTotal > 0 && percent > 0);

  // Deterministic reset-and-step: GRBL override bytes are RELATIVE
  // (+10 %, +1 % …), so a commit = reset to 100 % then step to the
  // target.  The worker stays stateless — only the UI knows the
  // slider's target.
  const writeFeedOverride = async (target: number) => {
    const clamped = Math.max(10, Math.min(200, Math.round(target)));
    const delta = clamped - 100;
    if (delta === 0) {
      await grblWriteByte(0x90); // set 100 %
      return;
    }
    await grblWriteByte(0x90);
    const step10 = delta > 0 ? 0x91 : 0x92;
    const step1 = delta > 0 ? 0x93 : 0x94;
    const tens = Math.floor(Math.abs(delta) / 10);
    const ones = Math.abs(delta) % 10;
    for (let i = 0; i < tens; i++) {
      await grblWriteByte(step10);
    }
    for (let i = 0; i < ones; i++) {
      await grblWriteByte(step1);
    }
  };

  // Power override steps in 10 % units (GRBL has no 1 % spindle step).
  const writePowerOverride = async (target: number) => {
    const clamped = Math.max(10, Math.min(100, Math.round(target)));
    const delta = clamped - 100;
    if (delta === 0) {
      await grblWriteByte(0x99); // set 100 %
      return;
    }
    await grblWriteByte(0x99);
    const step = delta > 0 ? 0x9a : 0x9b;
    const tens = Math.floor(Math.abs(delta) / 10);
    for (let i = 0; i < tens; i++) {
      await grblWriteByte(step);
    }
  };

  // Elapsed ticks while a job streams, freezes on completion, and
  // resets when the next job starts.
  useEffect(() => {
    if (!streaming) {
      return;
    }
    setElapsedSeconds(0);
    const startedAt = Date.now();
    const timer = window.setInterval(() => {
      setElapsedSeconds((Date.now() - startedAt) / 1000);
    }, 500);
    return () => {
      window.clearInterval(timer);
    };
  }, [streaming]);

  const formatDuration = (seconds: number) => {
    const total = Math.max(0, Math.round(seconds));
    const minutes = Math.floor(total / 60);
    const secs = total % 60;
    return `${String(minutes).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
  };

  // Connect is enabled once the selected transport has its inputs.
  const canConnect =
    connectMode === "tcp" ? host.trim().length > 0 : selectedPort !== null;

  // One button for both transports — connects or disconnects
  // whichever mode is active.
  const connectButton = (
    <button
      type="button"
      className="cad-action-primary h-9 flex-1 text-[10px] uppercase tracking-wider"
      disabled={busy || (!connected && !canConnect)}
      onClick={() => {
        if (connected) {
          void runCommand(() => grblDisconnect());
        } else {
          void connect();
        }
      }}
    >
      {connected
        ? t("cam.grbl.disconnect", "Disconnect")
        : t("cam.grbl.connect", "Connect")}
    </button>
  );

  return (
    <section
      className={`flex max-h-full min-h-0 flex-col overflow-y-auto ${
        embedded
          ? "h-full w-full px-4 py-4"
          : "pointer-events-auto cad-floating-panel w-[320px] max-w-full px-5 py-5"
      }`}
    >
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.grbl.title", "GRBL Machine")}</p>
        {embedded ? null : (
          <button
            type="button"
            className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
            onClick={onClose}
          >
            {t("cam.grbl.close", "Close")}
          </button>
        )}
      </div>

      {/* ── Connection ─────────────────────────────────────────── */}
      <div className="mt-4 space-y-3">
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("cam.grbl.transport", "Transport")}
          <Dropdown
            className="mt-2 w-full"
            value={connectMode}
            label={t("cam.grbl.transport", "Transport")}
            options={[
              {
                value: "serial",
                label: t("cam.grbl.serialTransport", "Serial (USB)"),
              },
              {
                value: "tcp",
                label: t("cam.grbl.tcpTransport", "Network (TCP)"),
              },
            ]}
            disabled={connected}
            onChange={(value) =>
              setConnectMode(value === "tcp" ? "tcp" : "serial")
            }
          />
        </label>

        {connectMode === "serial" ? (
          <>
            <div className="flex items-end gap-2">
              <label className="block min-w-0 flex-1 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.grbl.port", "Port")}
                <Dropdown
                  className="mt-2 w-full"
                  value={selectedPort ?? ""}
                  label={t("cam.grbl.port", "Port")}
                  options={
                    ports.length > 0
                      ? ports.map((port) => ({
                          value: port.name,
                          label: port.name,
                        }))
                      : [
                          {
                            value: "",
                            label: t("cam.grbl.noPorts", "No serial ports found"),
                          },
                        ]
                  }
                  disabled={connected || ports.length === 0}
                  onChange={(value) => setSelectedPort(value)}
                />
              </label>
              <button
                type="button"
                className="cad-action-ghost h-9 px-2 text-[10px] uppercase tracking-wider"
                disabled={connected || busy}
                onClick={() => {
                  void refreshPorts();
                }}
              >
                {t("cam.grbl.refreshPorts", "Refresh")}
              </button>
            </div>

            <div className="flex items-end gap-2">
              <label className="block min-w-0 flex-1 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.grbl.baudRate", "Baud rate")}
                <Dropdown
                  className="mt-2 w-full"
                  value={baudRate}
                  label={t("cam.grbl.baudRate", "Baud rate")}
                  options={BAUD_RATES.map((rate) => ({
                    value: String(rate),
                    label: String(rate),
                  }))}
                  disabled={connected}
                  onChange={(value) => setBaudRate(value)}
                />
              </label>
              {connectButton}
            </div>
          </>
        ) : (
          <>
            <div className="flex items-end gap-2">
              <label className="block min-w-0 flex-1 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.grbl.host", "Host")}
                <input
                  type="text"
                  className="cad-input mt-2 w-full font-mono"
                  value={host}
                  disabled={connected || busy}
                  placeholder="e.g. 192.168.1.19"
                  onChange={(event) => setHost(event.target.value)}
                />
              </label>
              <label className="block w-24 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.grbl.port", "Port")}
                <input
                  type="number"
                  className="cad-input mt-2 w-full font-mono"
                  value={tcpPort}
                  disabled={connected || busy}
                  onChange={(event) => setTcpPort(event.target.value)}
                />
              </label>
            </div>

            <div className="flex items-end gap-2">{connectButton}</div>
          </>
        )}
      </div>

      {/* ── Status ─────────────────────────────────────────────── */}
      <div className="mt-4 rounded-md border border-[var(--cad-panel-soft-border)] bg-[var(--cad-panel-soft-bg)] px-3 py-2">
        <div className="flex items-center justify-between text-xs">
          <span className="uppercase tracking-[0.18em] text-on-surface-muted">
            {t("cam.grbl.status", "Status")}
          </span>
          <span
            className={`font-mono ${
              connected ? "text-on-surface" : "text-on-surface-dim"
            }`}
          >
            {connected
              ? machineState ?? t("cam.grbl.connecting", "…")
              : t("cam.grbl.disconnectedState", "Offline")}
          </span>
        </div>
        {machineState === "alarm" ? (
          <p className="mt-1 text-[10px] text-danger">
            {t("cam.grbl.alarmHint", "Alarm — press Reset, then Unlock ($X).")}
          </p>
        ) : null}
        {mpos && (mpos[0] < 0 || mpos[1] < 0) ? (
          <p className="mt-1 text-[10px] text-on-surface-dim">
            {t(
              "cam.grbl.outsideWorkArea",
              "Machine position is outside the work area — jog inside, then press Zero XY.",
            )}
          </p>
        ) : null}
        <div className="mt-1 flex items-center justify-between font-mono text-[10px] text-on-surface-dim">
          <span>{t("cam.grbl.wcsPosition", "WCS")}</span>
          <span>{wcsPosition ? formatPosition(wcsPosition) : "—"}</span>
        </div>
        <div className="mt-1 flex items-center justify-between font-mono text-[10px] text-on-surface-dim">
          <span>{t("cam.grbl.mcsPosition", "MCS")}</span>
          <span>{mpos ? formatPosition(mpos) : "—"}</span>
        </div>
        <button
          type="button"
          className="cad-action-ghost mt-2 w-full"
          // FluidNC locks G-code while jogging — zeroing is rejected
          // until the jog finishes and the state returns to Idle.
          disabled={!connected || busy || machineState === "jog"}
          onClick={() => {
            void runCommand(() => grblZeroXy());
          }}
        >
          {t("cam.grbl.zeroXY", "Zero XY")}
        </button>
      </div>

      {/* ── Streaming ──────────────────────────────────────────── */}
      <div className="mt-4 space-y-3">
        <div className="grid grid-cols-2 gap-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={busy}
            onClick={() => {
              void loadFile();
            }}
          >
            {t("cam.grbl.loadFile", "Load G-code…")}
          </button>
          <button
            type="button"
            className="cad-action-primary"
            disabled={
              !connected || (!loadedPath && !loadedProgram) || streaming || busy
            }
            onClick={() => {
              onCycleStart?.();
              if (loadedProgram) {
                void runCommand(() => grblSendProgram(loadedProgram.text));
              } else if (loadedPath) {
                void runCommand(() => grblSendFile(loadedPath));
              }
            }}
          >
            {t("cam.grbl.start", "Cycle Start")}
          </button>
        </div>
        {loadedProgram ? (
          <div className="truncate font-mono text-[10px] text-on-surface-dim">
            {loadedProgram.label}
          </div>
        ) : loadedPath ? (
          <div className="truncate font-mono text-[10px] text-on-surface-dim">
            {loadedPath.split(/[\\/]/).pop()}
          </div>
        ) : null}

        {showProgress ? (
          <div>
            <div className="flex items-center justify-between font-mono text-[10px] text-on-surface-dim">
              <span>
                {linesSent.toLocaleString()} / {linesTotal.toLocaleString()}
              </span>
              <span>{percent.toFixed(0)}%</span>
            </div>
            <div className="mt-1 h-1.5 w-full overflow-hidden rounded-full bg-[var(--cad-loader-ring)]">
              <div
                className="h-full rounded-full bg-[var(--cad-accent-surface-strong)] transition-[width] duration-150"
                style={{ width: `${Math.min(100, Math.max(0, percent))}%` }}
              />
            </div>
            <div className="mt-1 flex items-center justify-between font-mono text-[10px] text-on-surface-dim">
              <span>
                {t("cam.grbl.elapsed", "Elapsed")}{" "}
                {formatDuration(elapsedSeconds)}
              </span>
              {streaming && percent > 0 ? (
                <span>
                  {t("cam.grbl.eta", "ETA")}{" "}
                  {formatDuration(elapsedSeconds * (100 / percent - 1))}
                </span>
              ) : null}
            </div>
          </div>
        ) : null}

        {/* ── Live overrides (real-time bytes, P5) ──────────────── */}
        <OverrideSlider
          label={t("cam.grbl.feedOverride", "Feed override")}
          value={feedOverride}
          min={10}
          max={200}
          disabled={!connected || busy}
          onChange={setFeedOverride}
          onCommit={(target) => {
            void runCommand(() => writeFeedOverride(target));
          }}
        />
        <OverrideSlider
          label={t("cam.grbl.powerOverride", "Power override")}
          value={powerOverride}
          min={10}
          max={100}
          disabled={!connected || busy}
          onChange={setPowerOverride}
          onCommit={(target) => {
            void runCommand(() => writePowerOverride(target));
          }}
        />
        <p className="text-[10px] leading-snug text-on-surface-dim">
          {t(
            "cam.grbl.overrideCaveat",
            "Real-time override support varies by firmware — FluidNC implements only part of it.",
          )}
        </p>

        <div className="grid grid-cols-2 gap-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={!streaming || paused || busy}
            onClick={() => {
              void runCommand(() => grblPause());
            }}
          >
            {t("cam.grbl.pause", "Pause")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={!streaming || !paused || busy}
            onClick={() => {
              void runCommand(() => grblResume());
            }}
          >
            {t("cam.grbl.resume", "Resume")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblReset());
            }}
          >
            {t("cam.grbl.reset", "Reset")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblHome());
            }}
          >
            {t("cam.grbl.home", "Home")}
          </button>
          <button
            type="button"
            className="cad-action-ghost col-span-2"
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblUnlock());
            }}
          >
            {t("cam.grbl.unlock", "Unlock ($X)")}
          </button>
        </div>
      </div>

      {/* ── Jog ────────────────────────────────────────────────── */}
      <fieldset className="mt-4 space-y-3 border-t border-[var(--cad-panel-soft-border)] pt-3">
        <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
          {t("cam.grbl.jog", "Jog")}
        </legend>
        <div className="flex items-end gap-2">
          <label className="flex-1 text-[10px] uppercase tracking-wider text-on-surface-muted">
            {t("cam.grbl.jogStep", "Step (mm)")}
            <input
              type="number"
              className="cad-input mt-1 w-full"
              value={jogStep}
              disabled={!connected || busy}
              onChange={(event) =>
                setJogStep(Math.max(0.1, Number(event.target.value)))
              }
            />
          </label>
          <label className="flex-1 text-[10px] uppercase tracking-wider text-on-surface-muted">
            {t("cam.grbl.jogFeed", "Feed (mm/min)")}
            <input
              type="number"
              className="cad-input mt-1 w-full"
              value={jogFeed}
              disabled={!connected || busy}
              onChange={(event) =>
                setJogFeed(Math.max(10, Number(event.target.value)))
              }
            />
          </label>
        </div>
        <div className="mx-auto grid w-36 grid-cols-3 gap-1.5">
          <span />
          <JogButton
            label={t("cam.grbl.jogYPlus", "+Y")}
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblJog(null, jogStep, jogFeed));
            }}
          />
          <span />
          <JogButton
            label={t("cam.grbl.jogXMinus", "-X")}
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblJog(-jogStep, null, jogFeed));
            }}
          />
          <span className="flex h-9 items-center justify-center rounded-md bg-[var(--cad-panel-soft-bg)] font-mono text-[10px] text-on-surface-dim">
            {t("cam.grbl.jogCenter", "XY")}
          </span>
          <JogButton
            label={t("cam.grbl.jogXPlus", "+X")}
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblJog(jogStep, null, jogFeed));
            }}
          />
          <span />
          <JogButton
            label={t("cam.grbl.jogYMinus", "-Y")}
            disabled={!connected || busy}
            onClick={() => {
              void runCommand(() => grblJog(null, -jogStep, jogFeed));
            }}
          />
          <span />
        </div>
      </fieldset>

      {/* ── Console ─────────────────────────────────────────────── */}
      <fieldset className="mt-4 space-y-3 border-t border-[var(--cad-panel-soft-border)] pt-3">
        <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
          {t("cam.grbl.command", "Command")}
        </legend>
        <div className="flex gap-2">
          <input
            type="text"
            className="cad-input min-w-0 flex-1"
            value={rawCommand}
            disabled={!connected || busy}
            placeholder="$20=0"
            onChange={(event) => setRawCommand(event.target.value)}
            onKeyDown={(event) => {
              // Up/down recall the sent-command history.
              if (event.key === "ArrowUp") {
                event.preventDefault();
                const current = historyIndex ?? commandHistory.length;
                const next = current - 1;
                if (next >= 0) {
                  setHistoryIndex(next);
                  setRawCommand(commandHistory[next]);
                }
                return;
              }
              if (event.key === "ArrowDown") {
                event.preventDefault();
                const current = historyIndex ?? commandHistory.length;
                const next = current + 1;
                if (next < commandHistory.length) {
                  setHistoryIndex(next);
                  setRawCommand(commandHistory[next]);
                } else {
                  setHistoryIndex(null);
                  setRawCommand("");
                }
                return;
              }
              if (event.key !== "Enter" || !connected || busy) {
                return;
              }
              const command = rawCommand.trim();
              if (!command) {
                return;
              }
              void runCommand(() => grblSendRaw(command));
              setCommandHistory((previous) => [...previous, command]);
              setHistoryIndex(null);
              setRawCommand("");
            }}
          />
          <button
            type="button"
            className="cad-action-ghost"
            disabled={!connected || busy || !rawCommand.trim()}
            onClick={() => {
              const command = rawCommand.trim();
              if (!command) {
                return;
              }
              void runCommand(() => grblSendRaw(command));
              setCommandHistory((previous) => [...previous, command]);
              setHistoryIndex(null);
              setRawCommand("");
            }}
          >
            {t("cam.grbl.send", "Send")}
          </button>
        </div>
        <button
          type="button"
          className="cad-action-ghost h-8 w-full"
          disabled={!connected || busy}
          onClick={() => {
            setSettingsOpen(true);
            void grblGetSettings().catch((error) => {
              useToastStore.getState().pushToast("error", String(error));
            });
          }}
        >
          {t("cam.grbl.settingsButton", "GRBL settings…")}
        </button>
        {lastMessage ? (
          <p className="font-mono text-[10px] leading-snug text-on-surface-dim">
            {lastMessage}
          </p>
        ) : null}
      </fieldset>

      {settingsOpen ? (
        <GrblSettingsDialog
          settings={settings}
          onClose={() => {
            setSettingsOpen(false);
          }}
          onApply={(key, value) => {
            void runCommand(() => grblSendRaw(`${key}=${value}`));
          }}
        />
      ) : null}
    </section>
  );
}

function JogButton({
  label,
  disabled,
  onClick,
}: {
  label: string;
  disabled: boolean;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      className="cad-action-ghost h-9 font-mono text-[10px]"
      disabled={disabled}
      onClick={onClick}
    >
      {label}
    </button>
  );
}

// Slider that commits on release: dragging updates the label only;
// the real-time override bytes fire once, on pointer-up.
function OverrideSlider({
  label,
  value,
  min,
  max,
  disabled,
  onChange,
  onCommit,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  disabled: boolean;
  onChange: (value: number) => void;
  onCommit: (value: number) => void;
}) {
  return (
    <label className="block text-[10px] uppercase tracking-wider text-on-surface-muted">
      <span className="flex items-center justify-between">
        {label}
        <span className="font-mono text-on-surface-dim">{value}%</span>
      </span>
      <input
        type="range"
        className="mt-1 w-full"
        min={min}
        max={max}
        step={1}
        value={value}
        disabled={disabled}
        onChange={(event) => onChange(Number(event.target.value))}
        onPointerUp={(event) => onCommit(Number(event.currentTarget.value))}
      />
    </label>
  );
}
