import { useState } from "react";
import { useTranslation } from "react-i18next";

import {
  grblLaserPower,
  type GcodeBounds,
  type GrblUtilityRequest,
} from "@/lib/grblClient";
import { useToastStore } from "@/state";

// Laser utilities section in the GRBL workspace's left column (below
// the machine panel): red pointer toggle, hold-to-fire test, focus
// pulse, and job framing.  The workspace owns the orchestration (it
// renders the overlay and picks the stream target); this panel only
// renders inputs and issues requests.
interface GrblUtilitiesPanelProps {
  connected: boolean;
  pointerOn: boolean;
  onTogglePointer: () => void;
  /** Parsed bounds of the loaded program — the framing rectangle. */
  jobBounds: GcodeBounds | null;
  onRunUtility: (request: GrblUtilityRequest) => void;
  hasOverlay: boolean;
  onClearOverlay: () => void;
}

export function GrblUtilitiesPanel({
  connected,
  pointerOn,
  onTogglePointer,
  jobBounds,
  onRunUtility,
  hasOverlay,
  onClearOverlay,
}: GrblUtilitiesPanelProps) {
  const { t } = useTranslation();

  const [firePower, setFirePower] = useState(5);
  const [pulsePower, setPulsePower] = useState(5);
  const [pulseDuration, setPulseDuration] = useState(0.5);
  const [framePower, setFramePower] = useState(2);
  const [frameFeed, setFrameFeed] = useState(1500);
  const [frameMargin, setFrameMargin] = useState(0);

  const reportError = (error: unknown) => {
    useToastStore.getState().pushToast("error", String(error));
  };

  // Hold-to-fire: beam on while the button is pressed, off on release
  // (pointer capture is not needed — pointerleave also ends the beam).
  const setFire = (on: boolean) => {
    if (!connected) {
      return;
    }
    void grblLaserPower(on ? firePower : null).catch(reportError);
  };

  const pulse = () => {
    onRunUtility({
      kind: "focusPulse",
      powerPercent: pulsePower,
      durationSeconds: pulseDuration,
    });
  };

  const frameJob = () => {
    if (!jobBounds) {
      return;
    }
    onRunUtility({
      kind: "framing",
      x: jobBounds.minX - frameMargin,
      y: jobBounds.minY - frameMargin,
      width: jobBounds.maxX - jobBounds.minX + 2 * frameMargin,
      height: jobBounds.maxY - jobBounds.minY + 2 * frameMargin,
      powerPercent: framePower,
      feed: frameFeed,
    });
  };

  return (
    <section className="shrink-0 space-y-2 border-t border-[var(--cad-panel-soft-border)] bg-surface-lowest px-4 py-3">
      <p className="cad-kicker">{t("grbl.util.title")}</p>

      <div className="flex items-center gap-2">
        <button
          type="button"
          className="cad-action-ghost h-8 flex-1"
          disabled={!connected}
          onClick={onTogglePointer}
        >
          {pointerOn ? t("grbl.pointerOff") : t("grbl.pointerOn")}
        </button>
      </div>
      <p className="text-[10px] text-on-surface-dim">{t("grbl.pointerHint")}</p>

      <div className="flex items-center gap-2">
        <NumberField
          label={t("grbl.util.powerPercent")}
          value={firePower}
          disabled={!connected}
          onChange={setFirePower}
        />
        <button
          type="button"
          className="cad-action-ghost h-8 flex-1"
          title={t("grbl.util.testFireHint")}
          disabled={!connected}
          onPointerDown={() => setFire(true)}
          onPointerUp={() => setFire(false)}
          onPointerLeave={() => setFire(false)}
          onPointerCancel={() => setFire(false)}
          onContextMenu={(event) => {
            // A long press on some platforms opens the context menu
            // mid-fire — swallowing it keeps the beam under control.
            event.preventDefault();
          }}
        >
          {t("grbl.util.testFire")}
        </button>
      </div>
      <p className="text-[10px] text-on-surface-dim">
        {t("grbl.util.testFireHint")}
      </p>

      <div className="flex items-center gap-2">
        <NumberField
          label={t("grbl.util.powerPercent")}
          value={pulsePower}
          disabled={!connected}
          onChange={setPulsePower}
        />
        <NumberField
          label={t("grbl.util.durationSeconds")}
          value={pulseDuration}
          step={0.1}
          disabled={!connected}
          onChange={setPulseDuration}
        />
        <button
          type="button"
          className="cad-action-ghost h-8 flex-1"
          disabled={!connected}
          onClick={pulse}
        >
          {t("grbl.util.focusPulse")}
        </button>
      </div>

      <div className="flex items-center gap-2">
        <NumberField
          label={t("grbl.util.powerPercent")}
          value={framePower}
          disabled={!connected}
          onChange={setFramePower}
        />
        <NumberField
          label={t("grbl.util.feed")}
          value={frameFeed}
          disabled={!connected}
          onChange={setFrameFeed}
        />
        <NumberField
          label={t("grbl.util.margin")}
          value={frameMargin}
          disabled={!connected}
          onChange={setFrameMargin}
        />
        <button
          type="button"
          className="cad-action-ghost h-8 flex-1"
          disabled={!connected || !jobBounds}
          onClick={frameJob}
        >
          {t("grbl.util.framing")}
        </button>
      </div>
      {!jobBounds ? (
        <p className="text-[10px] text-on-surface-dim">
          {t("grbl.util.framingNoJob")}
        </p>
      ) : null}

      {hasOverlay ? (
        <button
          type="button"
          className="cad-action-ghost h-8 w-full"
          onClick={onClearOverlay}
        >
          {t("grbl.util.clearOverlay")}
        </button>
      ) : null}
    </section>
  );
}

function NumberField({
  label,
  value,
  step = 1,
  disabled,
  onChange,
}: {
  label: string;
  value: number;
  step?: number;
  disabled: boolean;
  onChange: (value: number) => void;
}) {
  return (
    <label className="min-w-0 flex-1 text-[10px] uppercase tracking-wider text-on-surface-muted">
      {label}
      <input
        type="number"
        step={step}
        className="cad-input mt-1 w-full font-mono"
        value={value}
        disabled={disabled}
        onChange={(event) => onChange(Number(event.target.value))}
      />
    </label>
  );
}
