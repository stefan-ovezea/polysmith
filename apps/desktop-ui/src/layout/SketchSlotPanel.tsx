import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type { SketchSlotEntry } from "@/types";
import { PreviewPanelActions } from "./PreviewPanelActions";

const RAD_TO_DEG = 180 / Math.PI;
const DEG_TO_RAD = Math.PI / 180;
const UPDATE_DEBOUNCE_MS = 250;

interface SketchSlotPanelProps {
  // Live slot record — the panel mirrors its parameters and edits
  // them via `update_sketch_slot` (the core re-expands the generated
  // stadium geometry on every recompute).
  slot: SketchSlotEntry;
  disabled: boolean;
  onUpdate: (
    length: number,
    radius: number,
    rotationRad: number,
  ) => Promise<void> | void;
  onConfirm: () => void | Promise<void>;
  // Close only — the slot itself stays (unlike the text panel's
  // delete-on-cancel; the user can delete it with the Delete key).
  onCancel: () => void | Promise<void>;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

// Parameter panel for a straight slot (stadium): length, radius, and
// rotation (shown in degrees, sent in radians). Every valid edit is
// debounced and sent as a full `update_sketch_slot` patch; the core
// enforces `length >= 2 * radius`, so the panel clamps the radius to
// half the length before sending.
export function SketchSlotPanel({
  slot,
  disabled,
  onUpdate,
  onConfirm,
  onCancel,
}: SketchSlotPanelProps) {
  const { t } = useTranslation();
  const [length, setLength] = useState(String(slot.length));
  const [radius, setRadius] = useState(String(slot.radius));
  const [rotationDeg, setRotationDeg] = useState(
    String(slot.rotation * RAD_TO_DEG),
  );
  const timerRef = useRef<number | null>(null);

  useEffect(() => {
    setLength(String(slot.length));
    setRadius(String(slot.radius));
    setRotationDeg(String(slot.rotation * RAD_TO_DEG));
  }, [slot.slot_id, slot.length, slot.radius, slot.rotation]);

  useEffect(() => {
    return () => {
      if (timerRef.current !== null) {
        window.clearTimeout(timerRef.current);
        timerRef.current = null;
      }
    };
  }, []);

  function scheduleUpdate(
    nextLength: string,
    nextRadius: string,
    nextRotationDeg: string,
  ) {
    if (timerRef.current !== null) {
      window.clearTimeout(timerRef.current);
    }
    timerRef.current = window.setTimeout(() => {
      timerRef.current = null;
      const parsedLength = Number(nextLength);
      const parsedRadius = Number(nextRadius);
      const parsedRotation = Number(nextRotationDeg);
      if (
        !isPositiveFiniteNumber(parsedLength) ||
        !isPositiveFiniteNumber(parsedRadius) ||
        !Number.isFinite(parsedRotation)
      ) {
        return;
      }
      // Core validation: length must stay >= 2 * radius.
      const clampedRadius = Math.min(parsedRadius, parsedLength / 2);
      if (clampedRadius !== parsedRadius) {
        setRadius(String(clampedRadius));
      }
      void onUpdate(parsedLength, clampedRadius, parsedRotation * DEG_TO_RAD);
    }, UPDATE_DEBOUNCE_MS);
  }

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">{t("panels.sketchSlot.title")}</p>
      <p className="mt-3 text-xs text-on-surface-muted">
        {t("panels.sketchSlot.helper")}
      </p>
      <form
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void onConfirm();
        }}
      >
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.sketchSlot.length")}
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={length}
            disabled={disabled}
            onChange={(event) => {
              setLength(event.target.value);
              scheduleUpdate(event.target.value, radius, rotationDeg);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.sketchSlot.radius")}
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={radius}
            disabled={disabled}
            onChange={(event) => {
              setRadius(event.target.value);
              scheduleUpdate(length, event.target.value, rotationDeg);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.sketchSlot.rotation")}
          <input
            className="cad-input mt-2"
            type="number"
            step="0.5"
            value={rotationDeg}
            disabled={disabled}
            onChange={(event) => {
              setRotationDeg(event.target.value);
              scheduleUpdate(length, radius, event.target.value);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <PreviewPanelActions
          confirmDisabled={disabled}
          cancelDisabled={disabled}
          onCancel={() => {
            void onCancel();
          }}
        />
        <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
          {t("panels.shortcutHint.confirm")}
        </p>
      </form>
    </section>
  );
}
