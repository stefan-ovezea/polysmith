import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

// Floating Transform / Array panel for the current sketch selection.
// v1 is commit-oriented (no live preview): Apply sends the transform
// or array command against the selected entities and the panel stays
// open for another run; Close dismisses it. Undo is the adjust path.

interface SketchTransformPanelProps {
  // Pre-filled center for rotate/scale/circular-array (the selection
  // centroid, computed by App when the panel opens).
  centerX: number;
  centerY: number;
  disabled: boolean;
  onApplyTransform: (params: {
    dx: number;
    dy: number;
    angleDeg: number;
    scale: number;
    copy: boolean;
    centerX: number;
    centerY: number;
  }) => Promise<void> | void;
  onApplyLinearArray: (params: {
    dx: number;
    dy: number;
    count: number;
  }) => Promise<void> | void;
  onApplyCircularArray: (params: {
    centerX: number;
    centerY: number;
    count: number;
    totalAngleDeg: number;
  }) => Promise<void> | void;
  // Pick-center mode: the next click in the viewport sets the center
  // (snapped to circle/arc centers, endpoints or the grid).
  pickingCenter: boolean;
  onPickCenter: () => void;
  onCancelPickCenter: () => void;
  // Live selection readout: human-readable kinds of the entities the
  // array/transform will apply to. Empty = nothing selected (the
  // action buttons disable).
  selectedKinds: string[];
  // Cancel = revert every entity this panel session created (arrays
  // and transform copies) and close; OK = keep everything and close.
  onConfirm: () => void;
  onCancel: () => void;
}

function numberOr(value: string, fallback: number) {
  // An empty or whitespace-only field parses to 0 — treat it as
  // "not set" so a briefly cleared input doesn't silently become 0
  // (which the count clamp would turn into 2).
  const trimmed = value.trim();
  if (trimmed === "") {
    return fallback;
  }
  const parsed = Number(trimmed);
  return Number.isFinite(parsed) ? parsed : fallback;
}

export function SketchTransformPanel({
  centerX,
  centerY,
  disabled,
  onApplyTransform,
  onApplyLinearArray,
  onApplyCircularArray,
  pickingCenter,
  onPickCenter,
  onCancelPickCenter,
  selectedKinds,
  onConfirm,
  onCancel,
}: SketchTransformPanelProps) {
  const { t } = useTranslation();
  const [dx, setDx] = useState("0");
  const [dy, setDy] = useState("0");
  const [angleDeg, setAngleDeg] = useState("0");
  const [scale, setScale] = useState("1");
  const [copy, setCopy] = useState(true);
  const [cx, setCx] = useState(String(centerX));
  const [cy, setCy] = useState(String(centerY));
  const [arrayCount, setArrayCount] = useState("3");
  const [totalAngleDeg, setTotalAngleDeg] = useState("360");

  // A picked center arrives as a prop while the panel is mounted.
  useEffect(() => {
    setCx(String(centerX));
    setCy(String(centerY));
  }, [centerX, centerY]);

  // An identity transform (0/0/0deg/1x) would copy the source onto
  // itself — invisible and pointless. Disable Apply in that case.
  const identityTransform =
    numberOr(dx, 0) === 0 &&
    numberOr(dy, 0) === 0 &&
    numberOr(angleDeg, 0) === 0 &&
    numberOr(scale, 1) === 1;
  // A zero-offset linear array stacks every copy on the source — the
  // copies exist but are invisible, which reads as "nothing happened".
  const zeroLinearOffset =
    numberOr(dx, 0) === 0 && numberOr(dy, 0) === 0;
  // Every action is a no-op without a live selection; disabling beats
  // a silently swallowed Apply.
  const noSelection = selectedKinds.length === 0;
  const actionsDisabled = disabled || noSelection;

  const fieldClass =
    "h-8 w-full min-w-0 flex-1 rounded-lg border border-outline/50 bg-surface-container-low px-2 text-sm text-on-surface";
  const labelClass = "w-16 text-xs text-on-surface-dim";
  const actionClass =
    "cad-tool-button cad-tool-button-active h-8 flex-1";

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        event.preventDefault();
        // While picking the center, Escape only leaves pick mode.
        if (pickingCenter) {
          onCancelPickCenter();
        } else {
          onCancel();
        }
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [onCancel, onCancelPickCenter, pickingCenter]);

  return (
    <div className="pointer-events-auto cad-context-menu absolute bottom-4 left-4 z-20 flex min-w-[260px] flex-col gap-2 p-3">
      <div className="flex items-center justify-between">
        <div className="text-sm font-medium text-on-surface">
          {t("panels.sketchTransform.title")}
        </div>
        <button
          type="button"
          className="cad-tool-button h-7 px-2 text-xs"
          onClick={onCancel}
        >
          {t("common.cancel")}
        </button>
        <button
          type="button"
          className="cad-tool-button h-7 px-2 text-xs"
          onClick={onConfirm}
        >
          {t("common.ok")}
        </button>
      </div>

      <div className="text-xs text-on-surface-dim">
        {noSelection
          ? t("panels.sketchTransform.noSelection")
          : t("panels.sketchTransform.selectionSummary", {
              count: selectedKinds.length,
              kinds: selectedKinds.join(", "),
            })}
      </div>

      <div className="flex items-center gap-2">
        <label className={labelClass}>{t("panels.sketchMove.dx")}</label>
        <input
          type="number"
          className={fieldClass}
          value={dx}
          onChange={(event) => setDx(event.target.value)}
        />
        <label className={labelClass}>{t("panels.sketchMove.dy")}</label>
        <input
          type="number"
          className={fieldClass}
          value={dy}
          onChange={(event) => setDy(event.target.value)}
        />
      </div>
      <div className="flex items-center gap-2">
        <label className={labelClass}>{t("panels.sketchMove.angle")}</label>
        <input
          type="number"
          className={fieldClass}
          value={angleDeg}
          onChange={(event) => setAngleDeg(event.target.value)}
        />
        <label className={labelClass}>{t("panels.sketchTransform.scale")}</label>
        <input
          type="number"
          step="0.1"
          className={fieldClass}
          value={scale}
          onChange={(event) => setScale(event.target.value)}
        />
      </div>
      <div className="flex items-center gap-2">
        <label className={labelClass}>{t("panels.sketchTransform.center")}</label>
        <button
          type="button"
          className={
            pickingCenter
              ? "cad-tool-button cad-tool-button-active h-8 flex-none px-2 text-xs"
              : "cad-tool-button h-8 flex-none px-2 text-xs"
          }
          onClick={pickingCenter ? onCancelPickCenter : onPickCenter}
        >
          {t("panels.sketchTransform.pickCenter")}
        </button>
        <input
          type="number"
          className={fieldClass}
          value={cx}
          onChange={(event) => setCx(event.target.value)}
        />
        <input
          type="number"
          className={fieldClass}
          value={cy}
          onChange={(event) => setCy(event.target.value)}
        />
      </div>
      <div className="text-xs text-on-surface-dim">
        {pickingCenter
          ? t("panels.sketchTransform.pickCenterHintActive")
          : t("panels.sketchTransform.pickCenterHint")}
      </div>
      <div className="flex items-center gap-3">
        <label className="flex items-center gap-1.5 text-xs text-on-surface-dim">
          <input
            type="checkbox"
            checked={copy}
            onChange={(event) => setCopy(event.target.checked)}
          />
          {t("panels.sketchTransform.copy")}
        </label>
        <button
          type="button"
          className={actionClass}
          disabled={actionsDisabled || identityTransform}
          onClick={() => {
            void onApplyTransform({
              dx: numberOr(dx, 0),
              dy: numberOr(dy, 0),
              angleDeg: numberOr(angleDeg, 0),
              scale: numberOr(scale, 1),
              copy,
              centerX: numberOr(cx, centerX),
              centerY: numberOr(cy, centerY),
            });
          }}
        >
          {t("panels.sketchTransform.applyTransform")}
        </button>
      </div>

      <div className="mt-2 border-t border-outline/40 pt-2">
        <div className="flex items-center gap-2">
          <label className={labelClass}>{t("panels.sketchTransform.count")}</label>
          <input
            type="number"
            className={fieldClass}
            value={arrayCount}
            onChange={(event) => setArrayCount(event.target.value)}
          />
          <button
            type="button"
            className={actionClass}
            disabled={actionsDisabled || zeroLinearOffset}
            onClick={() => {
              void onApplyLinearArray({
                dx: numberOr(dx, 0),
                dy: numberOr(dy, 0),
                count: Math.max(2, Math.round(numberOr(arrayCount, 3))),
              });
            }}
          >
            {t("panels.sketchTransform.linearArray")}
          </button>
        </div>
        <div className="text-xs text-on-surface-dim">
          {t("panels.sketchTransform.linearArrayHint")}
        </div>
        <div className="mt-2 flex items-center gap-2">
          <label className={labelClass}>{t("panels.sketchMove.angle")}</label>
          <input
            type="number"
            className={fieldClass}
            value={totalAngleDeg}
            onChange={(event) => setTotalAngleDeg(event.target.value)}
          />
          <button
            type="button"
            className={actionClass}
            disabled={actionsDisabled}
            onClick={() => {
              void onApplyCircularArray({
                centerX: numberOr(cx, centerX),
                centerY: numberOr(cy, centerY),
                count: Math.max(2, Math.round(numberOr(arrayCount, 3))),
                totalAngleDeg: numberOr(totalAngleDeg, 360),
              });
            }}
          >
            {t("panels.sketchTransform.circularArray")}
          </button>
        </div>
      </div>
    </div>
  );
}
