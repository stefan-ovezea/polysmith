import { Checkbox, Dropdown } from "@/lib";
import type { SketchTool, DimensionToolMode } from "@/types";
import type { CircleToolMode } from "./circleDraftPreview";
import { isDrawableSketchTool, sketchToolLabelKey } from "./draftDimensions";
import type { PolygonToolMode } from "./viewportPanelTypes";

type Translate = (key: string, options?: Record<string, unknown>) => string;

type ArcToolMode = "three_point" | "center_start_end";
type RectangleToolMode = "corner_corner" | "center_point" | "three_point";

interface SketchToolPanelProps {
  translate: Translate;
  activeSketchTool: SketchTool;
  sketchToolConstruction: boolean;
  arcToolMode: ArcToolMode;
  circleToolMode: CircleToolMode;
  rectangleToolMode: RectangleToolMode;
  polygonToolMode: PolygonToolMode;
  polygonSides: number;
  dimensionToolMode: DimensionToolMode;
  onConstructionChange: (checked: boolean) => void;
  onSetArcToolMode: (mode: ArcToolMode) => void;
  onSetCircleToolMode: (mode: CircleToolMode) => void;
  onSetRectangleToolMode: (mode: RectangleToolMode) => void;
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
  polygonToolMode,
  polygonSides,
  dimensionToolMode,
  onConstructionChange,
  onSetArcToolMode,
  onSetCircleToolMode,
  onSetRectangleToolMode,
  onSetPolygonToolMode,
  onSetDimensionToolMode,
  onPolygonSidesChange,
}: SketchToolPanelProps) {
  if (!isDrawableSketchTool(activeSketchTool) && activeSketchTool !== "dimension") {
    return null;
  }

  return (
    <section className="pointer-events-auto cad-floating-panel absolute right-4 top-4 z-20 w-72 px-5 py-5">
      <p className="cad-kicker">{translate("common.sketchTool")}</p>
      <h2 className="cad-title mt-2">
        {activeSketchTool === "dimension"
          ? translate("toolbar.dimension")
          : translate(sketchToolLabelKey(activeSketchTool))}
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
                  : "ThreePoint"
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
        {activeSketchTool !== "dimension" && (
        <label className="flex items-center justify-between gap-4 text-sm text-on-surface">
          <span>{translate("common.construction")}</span>
          <Checkbox
            checked={sketchToolConstruction}
            ariaLabel={translate("common.construction")}
            onCheckedChange={onConstructionChange}
          />
        </label>
        )}
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
                ]}
                label={translate("viewport.mode")}
                onChange={(value) => {
                  onSetRectangleToolMode(value as RectangleToolMode);
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
                  { value: "ordinate", label: translate("toolbar.dimensionOrdinate") },
                  { value: "jogged_radial", label: translate("toolbar.dimensionJoggedRadial") },
                  { value: "arc_length", label: translate("toolbar.dimensionArcLength") },
                  { value: "curve_min_max", label: translate("toolbar.dimensionCurveMinMax") },
                  { value: "baseline", label: translate("toolbar.dimensionBaseline") },
                  { value: "chain", label: translate("toolbar.dimensionChain") },
                  { value: "tidy_up", label: translate("toolbar.dimensionTidyUp") },
                  { value: "arrange", label: translate("toolbar.dimensionArrange") },
                  { value: "flip_arrows", label: translate("toolbar.dimensionFlipArrows") },
                  { value: "match", label: translate("toolbar.dimensionMatch") },
                  { value: "dimension_break", label: translate("toolbar.dimensionBreak") },
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
