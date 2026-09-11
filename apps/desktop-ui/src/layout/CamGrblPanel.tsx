import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { open } from "@tauri-apps/plugin-dialog";

import { Dropdown } from "@/lib";
import {
  grblConnect,
  grblConnectTcp,
  grblDisconnect,
  grblHome,
  grblJog,
  grblPause,
  grblReset,
  grblResume,
  grblSendFile,
  grblSendRaw,
  grblUnlock,
  grblZeroXy,
  listGrblPorts,
  type GrblPortInfo,
} from "@/lib/grblClient";
import { initGrblStreamListener, useGrblStore, useToastStore } from "@/state";
import { useCamEscapeCancel } from "./camPanelShared";

const BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400];

// Stable no-op for the embedded Escape handler (see below).
const NOOP = () => {};

// Direct GRBL transport panel.  The shell (gcode_sender.rs) owns the
// serial port; this panel only issues commands and renders the
// `grbl-stream` events from useGrblStore.  No document interaction —
// it streams already-exported .nc files, exactly like LaserGRBL.
//
// `embedded` turns this into the GRBL workspace's fixed left column:
// no Close button, no Escape-to-cancel, and a full-height plain panel
// instead of the floating card. `embeddedFilePath` (when set) makes
// Stream send that file directly instead of opening a picker — used by
// the CAM handoff. `onFileLoaded` lets the host (the workspace) learn
// about a disk-picked file so its preview parses the same program the
// panel will stream. The CAM setup entry passes neither optional prop.
export function CamGrblPanel({
  onClose,
  embedded,
  embeddedFilePath,
  onFileLoaded,
}: {
  onClose: () => void;
  embedded?: boolean;
  embeddedFilePath?: string | null;
  onFileLoaded?: (path: string) => void;
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
  // The program loaded for streaming.  Loading NEVER sends — Cycle
  // Start does.  In the GRBL workspace the host passes the previewed
  // file down; in the CAM panel Load picks it.
  const [loadedPath, setLoadedPath] = useState<string | null>(null);
  const [rawCommand, setRawCommand] = useState("");

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

  // The workspace/handoff path is the program to send; keep it in
  // sync so Cycle Start always sends what the preview shows.
  useEffect(() => {
    if (embeddedFilePath) {
      setLoadedPath(embeddedFilePath);
    }
  }, [embeddedFilePath]);

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
        setLoadedPath(selected);
        onFileLoaded?.(selected);
      }
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  const showProgress = streaming || (connected && linesTotal > 0 && percent > 0);

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
            disabled={!connected || !loadedPath || streaming || busy}
            onClick={() => {
              if (loadedPath) {
                void runCommand(() => grblSendFile(loadedPath));
              }
            }}
          >
            {t("cam.grbl.start", "Cycle Start")}
          </button>
        </div>
        {loadedPath ? (
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
          </div>
        ) : null}

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
              if (event.key !== "Enter" || !connected || busy) {
                return;
              }
              const command = rawCommand.trim();
              if (!command) {
                return;
              }
              void runCommand(() => grblSendRaw(command));
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
              setRawCommand("");
            }}
          >
            {t("cam.grbl.send", "Send")}
          </button>
        </div>
        {lastMessage ? (
          <p className="font-mono text-[10px] leading-snug text-on-surface-dim">
            {lastMessage}
          </p>
        ) : null}
      </fieldset>
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
