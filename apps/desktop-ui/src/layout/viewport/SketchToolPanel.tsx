import { Checkbox, Dropdown } from "@/lib";
import type { SketchTool, DimensionToolMode } from "@/types";
import type { CircleToolMode } from "./circleDraftPreview";
import { isDrawableSketchTool, sketchToolLabelKey } from "./draftDimensions";
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
  onSetSplineToolMode: (mode: "control" | "fit") => void;
  onSplineToolDegreeChange: (degree: number) => void;
  onSetPolygonToolMode: (mode: PolygonToolMode) => void;
  onSetDimensionToolMode: (mode: DimensionToolMode) => void;
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
  onSetSplineToolMode,
  onSplineToolDegreeChange,
  onSetPolygonToolMode,
  onSetDimensionToolMode,
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
      {activeSketchTool === "dimension" && (
        <p className="text-xs text-on-surface/50 mt-1">
          {translate(
            `toolbar.dimension${
              dimensionToolMode === "auto"
                ? "Auto"
                : dimensionToolMode === "linear"
                  ? "Linear"
                  : dimensionToolMode === "aligned"
                    ? "Aligned"
                    : dimensionToolMode === "angular"
                      ? "Angular"
                      : dimensionToolMode === "radius"
                        ? "Radius"
                        : dimensionToolMode === "diameter"
                          ? "Diameter"
                          : dimensionToolMode === "ordinate"
                            ? "Ordinate"
                            : dimensionToolMode === "jogged_radial"
                              ? "JoggedRadial"
                              : dimensionToolMode === "arc_length"
                                ? "ArcLength"
                                : dimensionToolMode === "curve_min_max"
                                  ? "CurveMinMax"
                                  : dimensionToolMode === "baseline"
                                    ? "Baseline"
                                    : dimensionToolMode === "chain"
                                      ? "Chain"
                                      : dimensionToolMode === "tidy_up"
                                        ? "TidyUp"
                                        : dimensionToolMode === "arrange"
                                          ? "Arrange"
                                          : dimensionToolMode === "flip_arrows"
                                            ? "FlipArrows"
                                            : dimensionToolMode === "match"
                                              ? "Match"
                                              : "Break"
            }`,
          )}
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
              <Dropdown
                value={
                  lineType !== ""
                    ? lineType
                    : sketchToolConstruction
                      ? "construction"
                      : "solid"
                }
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
                  // force construction in the core.
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
            <div className="mt-3 flex gap-2">
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
                      ? "cad-action-primary flex-1"
                      : "cad-action-ghost flex-1"
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
            <p className="cad-kicker">{translate("viewport.mode")}</p>
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
          </div>
        ) : null}
      </div>
    </section>
  );
}
