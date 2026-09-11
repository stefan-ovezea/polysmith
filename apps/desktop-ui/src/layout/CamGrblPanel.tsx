import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { open } from "@tauri-apps/plugin-dialog";

import { Dropdown } from "@/lib";
import {
  grblConnect,
  grblDisconnect,
  grblHome,
  grblJog,
  grblPause,
  grblReset,
  grblResume,
  grblSendFile,
  grblUnlock,
  listGrblPorts,
  type GrblPortInfo,
} from "@/lib/grblClient";
import { initGrblStreamListener, useGrblStore, useToastStore } from "@/state";
import { useCamEscapeCancel } from "./camPanelShared";

const BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400];

// Direct GRBL transport panel.  The shell (gcode_sender.rs) owns the
// serial port; this panel only issues commands and renders the
// `grbl-stream` events from useGrblStore.  No document interaction —
// it streams already-exported .nc files, exactly like LaserGRBL.
export function CamGrblPanel({ onClose }: { onClose: () => void }) {
  const { t } = useTranslation();

  const connected = useGrblStore((state) => state.connected);
  const streaming = useGrblStore((state) => state.streaming);
  const paused = useGrblStore((state) => state.paused);
  const linesSent = useGrblStore((state) => state.linesSent);
  const linesTotal = useGrblStore((state) => state.linesTotal);
  const percent = useGrblStore((state) => state.percent);
  const machineState = useGrblStore((state) => state.machineState);
  const mpos = useGrblStore((state) => state.mpos);

  const [ports, setPorts] = useState<GrblPortInfo[]>([]);
  const [selectedPort, setSelectedPort] = useState<string | null>(null);
  const [baudRate, setBaudRate] = useState("115200");
  const [jogStep, setJogStep] = useState(10);
  const [jogFeed, setJogFeed] = useState(1000);
  const [busy, setBusy] = useState(false);

  useCamEscapeCancel(onClose);

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
    if (!selectedPort) {
      return;
    }
    await runCommand(() => grblConnect(selectedPort, Number(baudRate)));
  };

  const streamFile = async () => {
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
        await runCommand(() => grblSendFile(selected));
      }
    } catch (error) {
      useToastStore.getState().pushToast("error", String(error));
    }
  };

  const showProgress = streaming || (connected && linesTotal > 0 && percent > 0);

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.grbl.title", "GRBL Machine")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          onClick={onClose}
        >
          {t("cam.grbl.close", "Close")}
        </button>
      </div>

      {/* ── Connection ─────────────────────────────────────────── */}
      <div className="mt-4 space-y-3">
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
          <button
            type="button"
            className="cad-action-primary h-9 flex-1 text-[10px] uppercase tracking-wider"
            disabled={busy || (!connected && !selectedPort)}
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
        </div>
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
        <div className="mt-1 flex items-center justify-between font-mono text-[10px] text-on-surface-dim">
          <span>{t("cam.grbl.position", "Position")}</span>
          <span>
            {mpos
              ? `X ${mpos[0].toFixed(2)}  Y ${mpos[1].toFixed(2)}  Z ${mpos[2].toFixed(2)}`
              : "—"}
          </span>
        </div>
      </div>

      {/* ── Streaming ──────────────────────────────────────────── */}
      <div className="mt-4 space-y-3">
        <button
          type="button"
          className="cad-action-primary w-full"
          disabled={!connected || streaming || busy}
          onClick={() => {
            void streamFile();
          }}
        >
          {t("cam.grbl.streamFile", "Stream G-code file…")}
        </button>

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
