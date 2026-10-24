import { Checkbox, Dropdown } from "@/lib";
import type { SketchTool, DimensionToolMode, SketchDiagnosticsEvent } from "@/types";
import type { CircleToolMode } from "./circleDraftPreview";
import { isDrawableSketchTool, sketchToolLabelKey } from "./draftDimensions";
import { lineTypeFlagsToSelection } from "./lineTypeSelection";
import { DIMENSION_MODE_LABEL_SUFFIX, dimensionModeHintKey } from "./dimensionModeRules";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import type { SlotToolMode } from "./slotDraftPreview";
import type { PolygonToolMode } from "./viewportPanelTypes";

type Translate = (key: string, options?: Record<string, unknown>) => string;

type ArcToolMode = "three_point" | "center_start_end";

interface SketchToolPanelProps {
  translate: Translate;
  activeSketchTool: SketchTool;
  sketchToolConstruction: boolean;
  arcToolMode: ArcToolMode;
  circleToolMode: CircleToolMode;
  rectangleToolMode: RectangleToolMode;
  rectangleCornerRadius: number;
  /** Line tool reference-line flavour ("" = solid). */
  lineType: "" | "centerline" | "infinite";
  slotToolMode: SlotToolMode;
  /** T2.3: straight slot or arc slot (centreline arc). */
  slotType: "straight" | "arc";
  /** Arc slot sweep in degrees (panel value). */
  slotArcSweepDegrees: number;
  /** Spline creation mode (T2.1): control points or fit points. */
  splineToolMode: "control" | "fit";
  /** Requested spline degree (1..5). */
  splineToolDegree: number;
  polygonToolMode: PolygonToolMode;
  polygonSides: number;
  dimensionToolMode: DimensionToolMode;
  onConstructionChange: (checked: boolean) => void;
  onSetArcToolMode: (mode: ArcToolMode) => void;
  onSetCircleToolMode: (mode: CircleToolMode) => void;
  onSetRectangleToolMode: (mode: RectangleToolMode) => void;
  onRectangleCornerRadiusChange: (radius: number) => void;
  onLineTypeChange: (lineType: "" | "centerline" | "infinite") => void;
  onSetSlotToolMode: (mode: SlotToolMode) => void;
  onSlotTypeChange: (slotType: "straight" | "arc") => void;
  onSlotArcSweepChange: (degrees: number) => void;
  onSetSplineToolMode: (mode: "control" | "fit") => void;
  onSplineToolDegreeChange: (degree: number) => void;
  onSetPolygonToolMode: (mode: PolygonToolMode) => void;
  onSetDimensionToolMode: (mode: DimensionToolMode) => void;
  /** S5: loose ends of the sketch being drawn (null until fetched). */
  sketchDiagnostics: SketchDiagnosticsEvent["payload"] | null;
  /** Tie every untied pair with a coincident constraint. */
  onTieEnds: () => void;
  /** S11: infer and apply constraints, then dimension what is left. */
  onFullyConstrain: () => void;
  onPolygonSidesChange: (sides: number) => void;
}

export function SketchToolPanel({
  translate,
  activeSketchTool,
  sketchToolConstruction,
  arcToolMode,
  circleToolMode,
  rectangleToolMode,
  rectangleCornerRadius,
  lineType,
  slotToolMode,
  slotType,
  slotArcSweepDegrees,
  splineToolMode,
  splineToolDegree,
  polygonToolMode,
  polygonSides,
  dimensionToolMode,
  onConstructionChange,
  onSetArcToolMode,
  onSetCircleToolMode,
  onSetRectangleToolMode,
  onRectangleCornerRadiusChange,
  onLineTypeChange,
  onSetSlotToolMode,
  onSlotTypeChange,
  onSlotArcSweepChange,
  onSetSplineToolMode,
  onSplineToolDegreeChange,
  onSetPolygonToolMode,
  onSetDimensionToolMode,
  sketchDiagnostics,
  onTieEnds,
  onFullyConstrain,
  onPolygonSidesChange,
}: SketchToolPanelProps) {
  // The spline tool draws a draft preview but is deliberately not part of
  // isDrawableSketchTool (that union drives the dimension-entry fields, which a
  // spline has none of), so it needs its own pass-through here.
  const isSplineTool = (activeSketchTool as SketchTool) === "spline";
  if (
    !isDrawableSketchTool(activeSketchTool) &&
    activeSketchTool !== "dimension" &&
    !isSplineTool
  ) {
    return null;
  }

  return (
    <section className="pointer-events-auto cad-floating-panel absolute right-4 top-4 z-20 w-72 px-5 py-5">
      <p className="cad-kicker">{translate("common.sketchTool")}</p>
      <h2 className="cad-title mt-2">
        {activeSketchTool === "dimension"
          ? translate("toolbar.dimension")
          : isSplineTool
            ? translate("toolbar.spline")
            : translate(
                sketchToolLabelKey(
                  activeSketchTool as Parameters<typeof sketchToolLabelKey>[0],
                ),
              )}
      </h2>
      {activeSketchTool === "circle" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            `toolbar.circle${
              circleToolMode === "center_radius"
                ? "CenterRadius"
                : circleToolMode === "two_point"
                  ? "TwoPoint"
                  : circleToolMode === "three_point"
                    ? "ThreePoint"
                    : circleToolMode === "tangent_two_lines"
                      ? "TangentTwoLines"
                      : "TangentThreeLines"
            }`,
          )}
        </p>
      )}
      {activeSketchTool === "rectangle" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            `toolbar.rectangle${
              rectangleToolMode === "corner_corner"
                ? "CornerCorner"
                : rectangleToolMode === "center_point"
                  ? "CenterPoint"
                  : rectangleToolMode === "three_point"
                    ? "ThreePoint"
                    : "Rounded"
            }`,
          )}
        </p>
      )}
      {activeSketchTool === "slot" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            `toolbar.slot${
              slotToolMode === "center_point"
                ? "CenterPoint"
                : slotToolMode === "center_to_center"
                  ? "CenterToCenter"
                  : "Overall"
            }`,
          )}
        </p>
      )}
      {activeSketchTool === "arc" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            arcToolMode === "three_point"
              ? "toolbar.arcThreePoint"
              : "toolbar.arcCenter",
          )}
        </p>
      )}
      {activeSketchTool === "polygon" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            `toolbar.polygon${
              polygonToolMode === "circumscribed"
                ? "Circumscribed"
                : polygonToolMode === "inscribed"
                  ? "Inscribed"
                  : "Edge"
            }`,
          )}
        </p>
      )}
      {/* S11: infer constraints and dimension what is left. The core keeps a
          candidate only if the solver's DOF count actually drops, and the whole
          pass is one undo step. */}
      <button
        type="button"
        className="cad-action-ghost mt-4 w-full px-2 py-1 text-xs"
        onClick={onFullyConstrain}
      >
        {translate("viewport.fullyConstrain")}
      </button>
      {/* S5: the sketch looks closed but the profile walk disagrees. Say where,
          and offer the one-click fix. */}
      {sketchDiagnostics &&
      (sketchDiagnostics.open_count > 0 ||
        sketchDiagnostics.untied_count > 0) ? (
        <div className="mt-4 rounded-xl border border-amber-400/30 bg-amber-400/5 px-3 py-2">
          <p className="cad-kicker">{translate("viewport.looseEndsTitle")}</p>
          <p className="mt-1 text-xs text-on-surface/70">
            {translate("viewport.looseEndsSummary", {
              open: sketchDiagnostics.open_count,
              untied: sketchDiagnostics.untied_count,
            })}
          </p>
          {sketchDiagnostics.untied_count > 0 ? (
            <button
              type="button"
              className="cad-action-ghost mt-2 w-full px-2 py-1 text-xs"
              onClick={onTieEnds}
            >
              {translate("viewport.tieEnds")}
            </button>
          ) : null}
        </div>
      ) : null}
      {activeSketchTool === "dimension" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(`toolbar.dimension${DIMENSION_MODE_LABEL_SUFFIX[dimensionToolMode]}`)}
        </p>
      )}
      <div className="mt-5 flex flex-col gap-4">
        {activeSketchTool !== "dimension" && activeSketchTool !== "line" && (
        <label className="flex items-center justify-between gap-4 text-sm text-on-surface">
          <span>{translate("common.construction")}</span>
          <Checkbox
            checked={sketchToolConstruction}
            ariaLabel={translate("common.construction")}
            onCheckedChange={onConstructionChange}
          />
        </label>
        )}
        {activeSketchTool === "line" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.lineType")}</p>
            <div className="mt-3">
              {/* A dropdown, not a segmented row: the panel is 288 px wide and
                  four labels ("Construction", "Centerline", "Infinite") do not
                  fit in one line. The four-way mapping lives in
                  lineTypeSelection.ts and is unit-tested. */}
              <Dropdown
                value={lineTypeFlagsToSelection({
                  lineType,
                  isConstruction: sketchToolConstruction,
                })}
                options={[
                  { value: "solid", label: translate("viewport.lineTypeSolid") },
                  {
                    value: "construction",
                    label: translate("viewport.lineTypeConstruction"),
                  },
                  {
                    value: "centerline",
                    label: translate("viewport.lineTypeCenterline"),
                  },
                  {
                    value: "infinite",
                    label: translate("viewport.lineTypeInfinite"),
                  },
                ]}
                label={translate("viewport.lineType")}
                onChange={(value) => {
                  // "solid" / "construction" are the ordinary line with the
                  // construction flag toggled; the two reference flavours
                  // force construction in the core, so the flag stays clear
                  // here (see lineTypeSelection.ts, which is unit-tested).
                  if (value === "centerline" || value === "infinite") {
                    onConstructionChange(false);
                    onLineTypeChange(value);
                    return;
                  }
                  onLineTypeChange("");
                  onConstructionChange(value === "construction");
                }}
              />
            </div>
          </div>
        ) : null}
        {activeSketchTool === "arc" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            {/* Two options are short in English, but this row must not clip a
                longer translation: wrap the row and let each button shrink and
                wrap its own label instead of overflowing the 288 px panel. */}
            <div className="mt-3 flex flex-wrap gap-2">
              {[
                {
                  value: "three_point" as const,
                  label: translate("toolbar.arcThreePointTitle"),
                },
                {
                  value: "center_start_end" as const,
                  label: translate("toolbar.arcCenter"),
                },
              ].map((option) => (
                <button
                  key={option.value}
                  type="button"
                  className={
                    arcToolMode === option.value
                      ? "cad-action-primary min-w-0 flex-1 basis-24 px-2 whitespace-normal break-words text-center leading-tight"
                      : "cad-action-ghost min-w-0 flex-1 basis-24 px-2 whitespace-normal break-words text-center leading-tight"
                  }
                  onClick={() => {
                    onSetArcToolMode(option.value);
                  }}
                >
                  {option.label}
                </button>
              ))}
            </div>
          </div>
        ) : null}
        {activeSketchTool === "circle" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            <div className="mt-3">
              <Dropdown
                value={circleToolMode}
                options={[
                  {
                    value: "center_radius",
                    label: translate("toolbar.circleCenterRadius"),
                  },
                  {
                    value: "two_point",
                    label: translate("toolbar.circleTwoPoint"),
                  },
                  {
                    value: "three_point",
                    label: translate("toolbar.circleThreePoint"),
                  },
                  {
                    value: "tangent_two_lines",
                    label: translate("toolbar.circleTangentTwoLines"),
                  },
                  {
                    value: "tangent_three_lines",
                    label: translate("toolbar.circleTangentThreeLines"),
                  },
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetCircleToolMode(value as CircleToolMode);
                }}
              />
            </div>
          </div>
        ) : null}
        {activeSketchTool === "rectangle" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            <div className="mt-3">
              <Dropdown
                value={rectangleToolMode}
                options={[
                  {
                    value: "corner_corner",
                    label: translate("toolbar.rectangleCornerCorner"),
                  },
                  {
                    value: "center_point",
                    label: translate("toolbar.rectangleCenterPoint"),
                  },
                  {
                    value: "three_point",
                    label: translate("toolbar.rectangleThreePoint"),
                  },
                  {
                    value: "rounded",
                    label: translate("toolbar.rectangleRounded"),
                  },
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetRectangleToolMode(value as RectangleToolMode);
                }}
              />
              {rectangleToolMode === "rounded" ? (
                <div className="mt-3 flex items-center gap-2">
                  <span className="text-xs text-on-surface/60">
                    {translate("forms.radiusMm")}
                  </span>
                  <input
                    type="number"
                    min="0.01"
                    step="0.5"
                    className="h-7 w-16 rounded-md border px-2 text-xs text-center tabular-nums bg-transparent"
                    style={{
                      border: "1px solid var(--cad-panel-border)",
                      color: "inherit",
                    }}
                    aria-label={translate("forms.radiusMm")}
                    value={rectangleCornerRadius}
                    onChange={(event) => {
                      onRectangleCornerRadiusChange(
                        Math.max(0.01, Number(event.target.value) || 0.01),
                      );
                    }}
                  />
                </div>
              ) : null}
            </div>
          </div>
        ) : null}
        {activeSketchTool === "slot" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.slotType")}</p>
            <div className="mt-3">
              <Dropdown
                value={slotType}
                options={[
                  {
                    value: "straight",
                    label: translate("toolbar.slotStraight"),
                  },
                  {
                    value: "arc",
                    label: translate("toolbar.slotArc"),
                  },
                ]}
                label={translate("viewport.slotType")}
                onChange={(value) => {
                  onSlotTypeChange(value as "straight" | "arc");
                }}
              />
            </div>
            {slotType === "arc" ? (
              <label className="mt-4 block">
                <span className="cad-kicker">{translate("viewport.slotSweep")}</span>
                <input
                  className="cad-input mt-2 w-full"
                  type="number"
                  min={1}
                  max={359}
                  step={5}
                  value={slotArcSweepDegrees}
                  onChange={(event) => {
                    const next = Number(event.target.value);
                    if (Number.isFinite(next)) {
                      onSlotArcSweepChange(
                        Math.min(359, Math.max(1, next)),
                      );
                    }
                  }}
                />
                <span className="mt-1 block text-xs text-on-surface/50">
                  {translate("viewport.slotArcHint")}
                </span>
              </label>
            ) : (
              <>
                <p className="cad-kicker mt-4">{translate("viewport.mode")}</p>
                <div className="mt-3">
                  <Dropdown
                    value={slotToolMode}
                    options={[
                      {
                        value: "center_point",
                        label: translate("toolbar.slotCenterPoint"),
                      },
                      {
                        value: "center_to_center",
                        label: translate("toolbar.slotCenterToCenter"),
                      },
                      {
                        value: "overall",
                        label: translate("toolbar.slotOverall"),
                      },
                    ]}
                    label={translate("viewport.mode")}
                    onChange={(value) => {
                      onSetSlotToolMode(value as SlotToolMode);
                    }}
                  />
                </div>
              </>
            )}
          </div>
        ) : null}
        {isSplineTool ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            <div className="mt-3">
              <Dropdown
                value={splineToolMode}
                options={[
                  {
                    value: "control",
                    label: translate("toolbar.splineControlPoints"),
                  },
                  {
                    value: "fit",
                    label: translate("toolbar.splineFitPoints"),
                  },
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetSplineToolMode(value as "control" | "fit");
                }}
              />
            </div>
            <p className="text-xs text-on-surface/50 mt-2">
              {translate(
                splineToolMode === "fit"
                  ? "viewport.splineFitHint"
                  : "viewport.splineControlHint",
              )}
            </p>
            <div className="mt-3">
              <Dropdown
                value={String(splineToolDegree)}
                options={["1", "2", "3", "4", "5"].map((degree) => ({
                  value: degree,
                  label: `${translate("viewport.splineDegree")} ${degree}`,
                }))}
                label={translate("viewport.splineDegree")}
                onChange={(value) => {
                  onSplineToolDegreeChange(Number(value));
                }}
              />
            </div>
          </div>
        ) : null}
        {activeSketchTool === "polygon" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            <div className="mt-3">
              <Dropdown
                value={polygonToolMode}
                options={[
                  {
                    value: "circumscribed",
                    label: translate("toolbar.polygonCircumscribed"),
                  },
                  {
                    value: "inscribed",
                    label: translate("toolbar.polygonInscribed"),
                  },
                  {
                    value: "edge",
                    label: translate("toolbar.polygonEdge"),
                  },
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetPolygonToolMode(value as PolygonToolMode);
                }}
              />
              <div className="mt-3 flex items-center gap-2">
                <span className="text-xs text-on-surface/60">Sides:</span>
                <input
                  type="number"
                  min="3"
                  max="48"
                  step="1"
                  className="h-7 w-16 rounded-md border px-2 text-xs text-center tabular-nums bg-transparent"
                  style={{
                    border: "1px solid var(--cad-panel-border)",
                    color: "inherit",
                  }}
                  value={polygonSides}
                  onChange={(event) => {
                    onPolygonSidesChange(
                      Math.max(3, Math.min(48, Number(event.target.value) || 3)),
                    );
                  }}
                />
              </div>
            </div>
          </div>
        ) : null}
        {activeSketchTool === "dimension" ? (
          <div>
            <p className="cad-kicker">{translate("viewport.mode")}</p>
            <div className="mt-3">
              <Dropdown
                value={dimensionToolMode}
                options={[
                  { value: "auto", label: translate("toolbar.dimensionAuto") },
                  { value: "linear", label: translate("toolbar.dimensionLinear") },
                  { value: "aligned", label: translate("toolbar.dimensionAligned") },
                  { value: "angular", label: translate("toolbar.dimensionAngular") },
                  { value: "radius", label: translate("toolbar.dimensionRadius") },
                  { value: "diameter", label: translate("toolbar.dimensionDiameter") },
                  { value: "arc_length", label: translate("toolbar.dimensionArcLength") },
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetDimensionToolMode(value as DimensionToolMode);
                }}
              />
            </div>
            {/* S1: say what the armed mode accepts, so a click that is
                ignored (a line in Radius mode) reads as a rule, not a bug. */}
            <p className="text-xs text-on-surface/50 mt-2">
              {translate(dimensionModeHintKey(dimensionToolMode))}
            </p>
          </div>
        ) : null}
      </div>
    </section>
  );
}
