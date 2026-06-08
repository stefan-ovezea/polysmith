import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Checkbox, Dropdown, ScrollArea } from "@/lib";
import type {
  ExtrudeAdvancedParameters,
  ExtrudeExtentMode,
  ExtrudeExtentType,
  ExtrudeFeatureParameters,
  ExtrudeMode,
  ExtrudeSideParameters,
  ExtrudeThinPlacement,
} from "@/types";
import { readNumberInputValue } from "./numberInput";

interface ExtrudeTargetBodyOption {
  id: string;
  label: string;
}

interface ExtrudePreviewPanelProps {
  phase?: "pending" | "active";
  initialDepth: number;
  initialMode: ExtrudeMode;
  initialParameters?: ExtrudeFeatureParameters | null;
  selectedProfileCount?: number;
  canCombineWithExistingBody: boolean;
  availableTargetBodies: ExtrudeTargetBodyOption[];
  selectedFaceTargetId?: string | null;
  initialTargetBodyId: string | null;
  previewError?: string | null;
  disabled: boolean;
  onPreviewDepth: (depth: number) => Promise<void>;
  onPreviewMode: (mode: ExtrudeMode) => Promise<void>;
  onPreviewTargetBody: (targetBodyId: string | null) => Promise<void>;
  onPreviewParameters?: (
    parameters: ExtrudeFeatureParameters | ExtrudeAdvancedParameters,
  ) => Promise<void>;
  onConfirm: (
    depth: number,
    mode: ExtrudeMode,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters,
  ) => void | Promise<void>;
  onCancel: () => Promise<void>;
}

function defaultSide(distance: number): ExtrudeSideParameters {
  return {
    extent_type: "distance",
    distance: Math.abs(distance),
    start_offset: 0,
    taper_angle_degrees: 0,
    target_reference_id: null,
  };
}

function advancedFromInitial(
  depth: number,
  params: ExtrudeFeatureParameters | null | undefined,
): ExtrudeAdvancedParameters {
  return {
    extent_mode: params?.extent_mode ?? "one_side",
    side1: params?.side1 ?? defaultSide(depth),
    side2: params?.side2 ?? null,
    thin: params?.thin ?? {
      enabled: false,
      thickness: 1,
      placement: "center",
    },
    operation: params?.operation ?? params?.mode ?? "new_body",
    intersect_result: params?.intersect_result ?? "replace_target",
  };
}

export function ExtrudePreviewPanel({
  phase = "active",
  initialDepth,
  initialMode,
  initialParameters = null,
  selectedProfileCount = 1,
  canCombineWithExistingBody,
  availableTargetBodies,
  selectedFaceTargetId = null,
  initialTargetBodyId,
  previewError = null,
  disabled,
  onPreviewDepth,
  onPreviewMode,
  onPreviewTargetBody,
  onPreviewParameters,
  onConfirm,
  onCancel,
}: ExtrudePreviewPanelProps) {
  const { t } = useTranslation();
  const [depth, setDepth] = useState(String(initialDepth));
  const [mode, setMode] = useState<ExtrudeMode>(initialMode);
  const [targetBodyId, setTargetBodyId] = useState<string | null>(
    initialTargetBodyId,
  );
  const [advanced, setAdvanced] = useState<ExtrudeAdvancedParameters>(() =>
    advancedFromInitial(initialDepth, initialParameters),
  );
  const lastPreviewedRef = useRef<number>(initialDepth);
  const onPreviewDepthRef = useRef(onPreviewDepth);
  const advancedRef = useRef(advanced);
  const primaryDistanceInputRef = useRef<HTMLInputElement | null>(null);
  const didFocusDistanceRef = useRef(false);

  useEffect(() => {
    onPreviewDepthRef.current = onPreviewDepth;
  }, [onPreviewDepth]);

  useEffect(() => {
    if (disabled || didFocusDistanceRef.current) {
      return;
    }
    const input = primaryDistanceInputRef.current;
    if (!input) {
      return;
    }
    didFocusDistanceRef.current = true;
    input.focus();
    input.select();
  }, [disabled]);

  useEffect(() => {
    advancedRef.current = advanced;
  }, [advanced]);

  useEffect(() => {
    if (!selectedFaceTargetId) {
      return;
    }
    const next = { ...advancedRef.current };
    if (next.side1.extent_type === "to_object") {
      next.side1 = {
        ...next.side1,
        target_reference_id: selectedFaceTargetId,
      };
      updateAdvanced(next);
      return;
    }
    if (next.side2?.extent_type === "to_object") {
      next.side2 = {
        ...next.side2,
        target_reference_id: selectedFaceTargetId,
      };
      updateAdvanced(next);
    }
  }, [selectedFaceTargetId]);

  function emitParameters(
    next: ExtrudeAdvancedParameters,
    depthOverride: number | null = null,
  ) {
    if (!onPreviewParameters) {
      return;
    }
    const parsedDepth = depthOverride ?? Number(depth);
    if (!Number.isFinite(parsedDepth) || parsedDepth === 0) {
      return;
    }
    if (next.side1.distance === 0 || next.side2?.distance === 0) {
      return;
    }
    if (initialParameters) {
      void onPreviewParameters({
        ...initialParameters,
        depth: parsedDepth,
        mode: next.operation,
        target_body_id: targetBodyId,
        ...next,
      });
    } else {
      void onPreviewParameters(next);
    }
  }

  function updateAdvanced(
    next: ExtrudeAdvancedParameters,
    depthOverride: number | null = null,
  ) {
    setAdvanced(next);
    emitParameters(next, depthOverride);
  }

  function updateSide(
    sideKey: "side1" | "side2",
    patch: Partial<ExtrudeSideParameters>,
  ) {
    const currentSide = sideKey === "side1" ? advanced.side1 : advanced.side2;
    const nextSide = { ...(currentSide ?? advanced.side1), ...patch };
    const next = { ...advanced, [sideKey]: nextSide };
    if (sideKey === "side1" && advanced.extent_mode === "symmetric") {
      next.side2 = nextSide;
    }
    if (patch.distance !== undefined && sideKey === "side1") {
      const signedDistance = Number(depth) < 0 ? -nextSide.distance : nextSide.distance;
      updateAdvanced(next, signedDistance);
      setDepth(String(signedDistance));
      if (phase === "active" && signedDistance !== 0) {
        void onPreviewDepthRef.current(signedDistance);
      }
      return;
    }
    updateAdvanced(next);
  }

  async function flushPendingDepth() {
    const parsed = Number(depth);
    if (!Number.isFinite(parsed) || parsed === 0) {
      return;
    }
    if (phase === "active" && parsed !== lastPreviewedRef.current) {
      lastPreviewedRef.current = parsed;
      await onPreviewDepthRef.current(parsed);
    }
  }

  async function handleConfirm() {
    await flushPendingDepth();
    const parsed = Number(depth);
    if (!Number.isFinite(parsed) || parsed === 0) {
      return;
    }
    await onConfirm(parsed, advanced.operation, targetBodyId, advanced);
  }

  const side2 =
    advanced.side2 ??
    ({
      ...advanced.side1,
      distance: advanced.side1.distance,
    } satisfies ExtrudeSideParameters);
  const activeMode = advanced.operation;
  const canJoinSelectedProfiles = selectedProfileCount > 1;
  const needsTarget =
    activeMode !== "new_body" &&
    (activeMode !== "join" || canCombineWithExistingBody);
  const hasPreviewError = Boolean(previewError);
  const problemInputClass = hasPreviewError
    ? "cad-input cad-input-error mt-2"
    : "cad-input mt-2";

  function canUseOperation(nextMode: ExtrudeMode) {
    if (nextMode === "new_body") {
      return true;
    }
    if (nextMode === "join") {
      return canCombineWithExistingBody || canJoinSelectedProfiles;
    }
    return canCombineWithExistingBody;
  }

  function renderExtentTypeSelect(
    sideKey: "side1" | "side2",
    side: ExtrudeSideParameters,
  ) {
    return (
      <Dropdown
        className="mt-2 w-full"
        value={side.extent_type}
        label={t("panels.extrude.extentType")}
        options={[
          { value: "distance", label: t("panels.extrude.distance") },
          { value: "through_all", label: t("panels.extrude.throughAll") },
          { value: "to_object", label: t("panels.extrude.toObject") },
          { value: "to_next", label: t("panels.extrude.toNext") },
        ]}
        disabled={disabled}
        onChange={(value) => {
          updateSide(sideKey, {
            extent_type: value as ExtrudeExtentType,
          });
        }}
      />
    );
  }

  function renderSideControls(
    sideKey: "side1" | "side2",
    side: ExtrudeSideParameters,
    title: string,
  ) {
    return (
      <div className="rounded-md border border-outline-variant/70 p-3">
        <p className="cad-kicker">{title}</p>
        <label className="mt-3 block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.extrude.extentType")}
          {renderExtentTypeSelect(sideKey, side)}
        </label>
        {side.extent_type === "distance" ? (
          <label className="mt-3 block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("forms.distanceMm")}
            <input
              ref={sideKey === "side1" ? primaryDistanceInputRef : undefined}
              className={problemInputClass}
              type="number"
              min="0.01"
              step="0.01"
              value={side.distance}
              disabled={disabled}
              onChange={(event) => {
                updateSide(sideKey, {
                  distance: readNumberInputValue(event.currentTarget),
                });
              }}
            />
          </label>
        ) : null}
        {(side.extent_type === "through_all" ||
          side.extent_type === "to_object") &&
        availableTargetBodies.length > 0 ? (
          <label className="mt-3 block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.extrude.targetBody")}
            {renderTargetBodyDropdown(
              side.target_reference_id ?? targetBodyId ?? "__recent",
              (nextValue) => {
                updateSide(sideKey, { target_reference_id: nextValue });
              },
            )}
          </label>
        ) : null}
        <div className="mt-3 grid grid-cols-2 gap-2">
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.extrude.startOffset")}
            <input
              className="cad-input mt-2"
              type="number"
              min="0"
              step="0.01"
              value={side.start_offset}
              disabled={disabled}
              onChange={(event) => {
                updateSide(sideKey, {
                  start_offset: readNumberInputValue(event.currentTarget),
                });
              }}
            />
          </label>
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.extrude.taper")}
            <input
              className={problemInputClass}
              type="number"
              step="0.1"
              value={side.taper_angle_degrees}
              disabled={disabled}
              onChange={(event) => {
                updateSide(sideKey, {
                  taper_angle_degrees: readNumberInputValue(event.currentTarget),
                });
              }}
            />
          </label>
        </div>
      </div>
    );
  }

  function renderTargetBodyDropdown(
    value: string,
    onChange: (targetBodyId: string | null) => void,
  ) {
    return (
      <Dropdown
        className="mt-2 w-full"
        value={value}
        label={t("panels.extrude.targetBody")}
        options={[
          { value: "__recent", label: t("panels.extrude.mostRecentBody") },
          ...availableTargetBodies.map((body) => ({
            value: body.id,
            label: body.label,
          })),
        ]}
        disabled={disabled}
        onChange={(selectedValue) => {
          onChange(selectedValue === "__recent" ? null : selectedValue);
        }}
      />
    );
  }

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[340px] max-w-full flex-col overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("panels.extrude.title")}</p>
      <div className="mt-3 py-1 text-xs uppercase tracking-[0.16em] text-on-surface-muted">
        {selectedProfileCount === 1
          ? t("panels.extrude.faceSelected", { count: selectedProfileCount })
          : t("panels.extrude.facesSelected", { count: selectedProfileCount })}
      </div>
      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
        onKeyDown={(event) => {
          if (event.key === "Escape") {
            if ((event.target as HTMLElement | null)?.closest(".cad-dropdown")) {
              return;
            }
            event.preventDefault();
            void onCancel();
          }
        }}
      >
        <ScrollArea
          className="min-h-0 flex-1"
          viewportClassName="space-y-3 pr-4"
        >
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.extrude.operation")}
            <Dropdown
              className="mt-2 w-full"
              value={advanced.operation}
              label={t("panels.extrude.operation")}
              options={[
                { value: "new_body", label: t("panels.extrude.newBody") },
                { value: "join", label: t("panels.extrude.join") },
                { value: "cut", label: t("panels.extrude.cut") },
                { value: "intersect", label: t("panels.extrude.intersect") },
              ]}
              disabled={disabled}
              onChange={(value) => {
                const nextMode = value as ExtrudeMode;
                if (!canUseOperation(nextMode)) {
                  return;
                }
                setMode(nextMode);
                updateAdvanced({ ...advanced, operation: nextMode });
                void onPreviewMode(nextMode);
              }}
            />
            {!canCombineWithExistingBody ? (
              <p className="mt-2 text-[10px] tracking-wide text-on-surface-dim normal-case">
                {t("panels.extrude.combineNeedsBody")}
              </p>
            ) : null}
          </label>

          {advanced.operation === "intersect" ? (
            <fieldset className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              <legend className="mb-2">{t("panels.extrude.result")}</legend>
              <div className="grid grid-cols-2 gap-2">
                {(["replace_target", "new_body"] as const).map((value) => (
                  <button
                    key={value}
                    type="button"
                    className={
                      advanced.intersect_result === value
                        ? "cad-action-primary"
                        : "cad-action-ghost"
                    }
                    disabled={disabled}
                    onClick={() => {
                      updateAdvanced({ ...advanced, intersect_result: value });
                    }}
                  >
                    {value === "replace_target"
                      ? t("panels.extrude.replaceTarget")
                      : t("panels.extrude.newBody")}
                  </button>
                ))}
              </div>
            </fieldset>
          ) : null}

          {needsTarget && availableTargetBodies.length > 1 ? (
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("panels.extrude.targetBody")}
              {renderTargetBodyDropdown(targetBodyId ?? "__recent", (nextValue) => {
                setTargetBodyId(nextValue);
                void onPreviewTargetBody(nextValue);
              })}
            </label>
          ) : null}

          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.extrude.extent")}
            <Dropdown
              className="mt-2 w-full"
              value={advanced.extent_mode}
              label={t("panels.extrude.extent")}
              options={[
                { value: "one_side", label: t("panels.extrude.oneSide") },
                { value: "symmetric", label: t("panels.extrude.symmetric") },
                { value: "two_sides", label: t("panels.extrude.twoSides") },
              ]}
              disabled={disabled}
              onChange={(value) => {
                const nextMode = value as ExtrudeExtentMode;
                updateAdvanced({
                  ...advanced,
                  extent_mode: nextMode,
                  side2:
                    nextMode === "one_side"
                      ? null
                      : advanced.side2 ?? advanced.side1,
                });
              }}
            />
          </label>

          <div className="space-y-3">
            {renderSideControls("side1", advanced.side1, t("panels.extrude.side1"))}
            {advanced.extent_mode === "two_sides"
              ? renderSideControls("side2", side2, t("panels.extrude.side2"))
              : null}
          </div>

          <fieldset className="rounded-md border border-outline-variant/70 p-3 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            <label className="flex items-center justify-between gap-3">
              <span>{t("panels.extrude.thin")}</span>
              <Checkbox
                checked={advanced.thin.enabled}
                disabled={disabled}
                ariaLabel={t("panels.extrude.thin")}
                onCheckedChange={(checked) => {
                  updateAdvanced({
                    ...advanced,
                    thin: { ...advanced.thin, enabled: checked },
                  });
                }}
              />
            </label>
            {advanced.thin.enabled ? (
              <div className="mt-3 space-y-3">
                <label className="block">
                  {t("panels.extrude.thickness")}
                  <input
                    className="cad-input mt-2"
                    type="number"
                    min="0.01"
                    step="0.01"
                    value={advanced.thin.thickness}
                    disabled={disabled}
                    onChange={(event) => {
                      updateAdvanced({
                        ...advanced,
                        thin: {
                          ...advanced.thin,
                          thickness: readNumberInputValue(event.currentTarget),
                        },
                      });
                    }}
                  />
                </label>
                <div className="grid grid-cols-3 gap-2">
                  {(["center", "inside", "outside"] as ExtrudeThinPlacement[]).map(
                    (placement) => (
                      <button
                        key={placement}
                        type="button"
                        className={
                          advanced.thin.placement === placement
                            ? "cad-action-primary"
                            : "cad-action-ghost"
                        }
                        disabled={disabled}
                        onClick={() => {
                          updateAdvanced({
                            ...advanced,
                            thin: { ...advanced.thin, placement },
                          });
                        }}
                      >
                        {t(`panels.extrude.${placement}`)}
                      </button>
                    ),
                  )}
                </div>
              </div>
            ) : null}
          </fieldset>

          {previewError ? (
            <p className="rounded-md border border-danger/40 px-3 py-2 text-xs normal-case tracking-normal text-danger">
              {t("panels.extrude.previewFailed", { message: previewError })}
            </p>
          ) : null}

        </ScrollArea>

        <div className="mt-4 flex shrink-0 gap-3">
          <button
            type="submit"
            className="cad-action-primary flex-1"
            disabled={
              disabled ||
              (phase === "pending" && selectedProfileCount === 0) ||
              Number(depth) === 0 ||
              !Number.isFinite(Number(depth)) ||
              hasPreviewError
            }
          >
            {t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-action-ghost flex-1"
            disabled={disabled}
            onClick={() => {
              void onCancel();
            }}
          >
            {t("common.cancel")}
          </button>
        </div>
      </form>
    </section>
  );
}
