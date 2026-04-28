import { useEffect, useState } from "react";
import type { FeatureEntry } from "@/types";

interface SelectedBoxEditorProps {
  feature: FeatureEntry | null;
  selectedSketchPointId?: string | null;
  selectedSketchEntityId?: string | null;
  selectedSketchDimensionId?: string | null;
  disabled: boolean;
  onSubmit: (
    featureId: string,
    width: number,
    height: number,
    depth: number,
  ) => Promise<void>;
  onRename: (featureId: string, name: string) => Promise<void>;
  onDelete: (featureId: string) => Promise<void>;
  onUpdateSketchLine: (
    lineId: string,
    startX: number,
    startY: number,
    endX: number,
    endY: number,
  ) => Promise<void>;
  onUpdateSketchCircle: (
    circleId: string,
    centerX: number,
    centerY: number,
    radius: number,
  ) => Promise<void>;
  onUpdateSketchDimension: (
    dimensionId: string,
    value: number,
  ) => Promise<void>;
  onUpdateSketchPoint: (pointId: string, x: number, y: number) => Promise<void>;
  onSetSketchPointFixed: (pointId: string, isFixed: boolean) => Promise<void>;
}

export function SelectedBoxEditor({
  feature,
  selectedSketchPointId,
  selectedSketchEntityId,
  selectedSketchDimensionId,
  disabled,
  onSubmit,
  onRename,
  onDelete,
  onUpdateSketchLine,
  onUpdateSketchCircle,
  onUpdateSketchDimension,
  onUpdateSketchPoint,
  onSetSketchPointFixed,
}: SelectedBoxEditorProps) {
  const [name, setName] = useState("");
  const [width, setWidth] = useState("");
  const [height, setHeight] = useState("");
  const [depth, setDepth] = useState("");
  const [lineStartX, setLineStartX] = useState("");
  const [lineStartY, setLineStartY] = useState("");
  const [lineEndX, setLineEndX] = useState("");
  const [lineEndY, setLineEndY] = useState("");
  const [circleCenterX, setCircleCenterX] = useState("");
  const [circleCenterY, setCircleCenterY] = useState("");
  const [circleRadius, setCircleRadius] = useState("");
  const [dimensionValue, setDimensionValue] = useState("");
  const [pointX, setPointX] = useState("");
  const [pointY, setPointY] = useState("");

  useEffect(() => {
    const selectedSketchLine = feature?.sketch_parameters?.lines.find(
      (line) => line.line_id === selectedSketchEntityId,
    );
    const selectedSketchCircle = feature?.sketch_parameters?.circles.find(
      (circle) => circle.circle_id === selectedSketchEntityId,
    );
    const selectedSketchPoint = feature?.sketch_parameters?.points.find(
      (point) => point.point_id === selectedSketchPointId,
    );
    const selectedSketchDimension =
      feature?.sketch_parameters?.dimensions.find(
        (dimension) => dimension.dimension_id === selectedSketchDimensionId,
      ) ??
      feature?.sketch_parameters?.dimensions.find(
        (dimension) => dimension.entity_id === selectedSketchEntityId,
      );

    if (!feature?.box_parameters) {
      setName(feature?.name ?? "");
      setWidth("");
      setHeight("");
      setDepth("");
      setLineStartX(
        selectedSketchLine ? String(selectedSketchLine.start_x) : "",
      );
      setLineStartY(
        selectedSketchLine ? String(selectedSketchLine.start_y) : "",
      );
      setLineEndX(selectedSketchLine ? String(selectedSketchLine.end_x) : "");
      setLineEndY(selectedSketchLine ? String(selectedSketchLine.end_y) : "");
      setCircleCenterX(
        selectedSketchCircle ? String(selectedSketchCircle.center_x) : "",
      );
      setCircleCenterY(
        selectedSketchCircle ? String(selectedSketchCircle.center_y) : "",
      );
      setCircleRadius(
        selectedSketchCircle ? String(selectedSketchCircle.radius) : "",
      );
      setDimensionValue(
        selectedSketchDimension ? String(selectedSketchDimension.value) : "",
      );
      setPointX(selectedSketchPoint ? String(selectedSketchPoint.x) : "");
      setPointY(selectedSketchPoint ? String(selectedSketchPoint.y) : "");
      if (selectedSketchPoint) {
        setLineStartX("");
        setLineStartY("");
        setLineEndX("");
        setLineEndY("");
        setCircleCenterX("");
        setCircleCenterY("");
        setCircleRadius("");
      }
      return;
    }

    setName(feature.name);
    setWidth(String(feature.box_parameters.width));
    setHeight(String(feature.box_parameters.height));
    setDepth(String(feature.box_parameters.depth));
    setDimensionValue("");
    setPointX("");
    setPointY("");
  }, [
    feature,
    selectedSketchEntityId,
    selectedSketchDimensionId,
    selectedSketchPointId,
  ]);

  if (!feature) {
    return (
      <section className="pointer-events-auto cad-floating-panel px-5 py-5">
        <p className="cad-kicker">Inspector</p>
        <h2 className="cad-title mt-2">Selected Feature</h2>
        <p className="mt-4 text-sm text-on-surface-muted">
          Select a feature from the browser, viewport, or timeline to inspect
          it.
        </p>
      </section>
    );
  }

  if (!feature.box_parameters) {
    const selectedSketchLine = feature.sketch_parameters?.lines.find(
      (line) => line.line_id === selectedSketchEntityId,
    );
    const selectedSketchCircle = feature.sketch_parameters?.circles.find(
      (circle) => circle.circle_id === selectedSketchEntityId,
    );
    const selectedSketchPoint = feature.sketch_parameters?.points.find(
      (point) => point.point_id === selectedSketchPointId,
    );
    const selectedSketchDimension =
      feature.sketch_parameters?.dimensions.find(
        (dimension) => dimension.dimension_id === selectedSketchDimensionId,
      ) ??
      feature.sketch_parameters?.dimensions.find(
        (dimension) => dimension.entity_id === selectedSketchEntityId,
      );
    const isViewportDimensionEditing =
      selectedSketchDimensionId !== null &&
      selectedSketchDimension?.dimension_id === selectedSketchDimensionId;
    return (
      <section className="pointer-events-auto cad-floating-panel px-5 py-5">
        <p className="cad-kicker">Inspector</p>
        <h2 className="cad-title mt-2">Selected Feature</h2>
        <div className="mt-4 space-y-3 text-sm">
          <p className="text-on-surface-muted">
            {feature.sketch_parameters
              ? `Sketch on ${feature.sketch_parameters.plane_id} with ${feature.sketch_parameters.lines.length} line${feature.sketch_parameters.lines.length === 1 ? "" : "s"} and ${feature.sketch_parameters.circles.length} circle${feature.sketch_parameters.circles.length === 1 ? "" : "s"}`
              : feature.extrude_parameters
                ? `Extrude from ${feature.extrude_parameters.profile_id} · depth ${feature.extrude_parameters.depth} mm`
                : feature.cylinder_parameters
                  ? `Cylinder: r ${feature.cylinder_parameters.radius} x h ${feature.cylinder_parameters.height} mm`
                  : "The selected feature is not editable as a box."}
          </p>
          {feature.sketch_parameters ? (
            <>
              <p className="text-on-surface-muted">
                Active tool: {feature.sketch_parameters.active_tool}
              </p>
              {selectedSketchPoint ? (
                <form
                  className="space-y-3 rounded-2xl border border-white/10 bg-white/5 p-3"
                  onSubmit={(event) => {
                    event.preventDefault();
                    void onUpdateSketchPoint(
                      selectedSketchPoint.point_id,
                      Number(pointX),
                      Number(pointY),
                    );
                  }}
                >
                  <p className="text-on-surface-muted">
                    Selected point {selectedSketchPoint.point_id} ·{" "}
                    {selectedSketchPoint.kind}
                    {selectedSketchPoint.is_fixed ? " · fixed" : ""}
                  </p>
                  <div className="grid grid-cols-2 gap-3">
                    <div className="rounded-xl border border-white/10 bg-white/5 px-3 py-2">
                      <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
                        X
                      </p>
                      <input
                        className="cad-input mt-1"
                        type="number"
                        step="0.01"
                        value={pointX}
                        onChange={(event) => {
                          setPointX(event.target.value);
                        }}
                        disabled={disabled || selectedSketchPoint.is_fixed}
                      />
                    </div>
                    <div className="rounded-xl border border-white/10 bg-white/5 px-3 py-2">
                      <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
                        Y
                      </p>
                      <input
                        className="cad-input mt-1"
                        type="number"
                        step="0.01"
                        value={pointY}
                        onChange={(event) => {
                          setPointY(event.target.value);
                        }}
                        disabled={disabled || selectedSketchPoint.is_fixed}
                      />
                    </div>
                  </div>
                  <button
                    className="cad-action-primary min-w-[140px]"
                    type="submit"
                    disabled={
                      disabled ||
                      selectedSketchPoint.is_fixed ||
                      !Number.isFinite(Number(pointX)) ||
                      !Number.isFinite(Number(pointY))
                    }
                  >
                    Move Point
                  </button>
                  <button
                    className="cad-action-primary min-w-[140px]"
                    type="button"
                    disabled={disabled}
                    onClick={() => {
                      void onSetSketchPointFixed(
                        selectedSketchPoint.point_id,
                        !selectedSketchPoint.is_fixed,
                      );
                    }}
                  >
                    {selectedSketchPoint.is_fixed ? "Unfix Point" : "Fix Point"}
                  </button>
                </form>
              ) : null}
              {selectedSketchLine ? (
                <form
                  className="space-y-3 rounded-2xl border border-white/10 bg-white/5 p-3"
                  onSubmit={(event) => {
                    event.preventDefault();
                    void onUpdateSketchLine(
                      selectedSketchLine.line_id,
                      Number(lineStartX),
                      Number(lineStartY),
                      Number(lineEndX),
                      Number(lineEndY),
                    );
                  }}
                >
                  <p className="text-on-surface-muted">
                    Selected line {selectedSketchLine.line_id}
                    {selectedSketchLine.constraint
                      ? ` · ${selectedSketchLine.constraint}`
                      : ""}
                  </p>
                  {selectedSketchDimension && !isViewportDimensionEditing ? (
                    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Driving Length
                      <div className="mt-2 flex gap-3">
                        <input
                          className="cad-input"
                          type="number"
                          min="0.01"
                          step="0.01"
                          value={dimensionValue}
                          onChange={(event) => {
                            setDimensionValue(event.target.value);
                          }}
                          disabled={disabled}
                        />
                        <button
                          className="cad-action-primary shrink-0"
                          type="button"
                          disabled={disabled || Number(dimensionValue) <= 0}
                          onClick={() => {
                            void onUpdateSketchDimension(
                              selectedSketchDimension.dimension_id,
                              Number(dimensionValue),
                            );
                          }}
                        >
                          Drive
                        </button>
                      </div>
                    </label>
                  ) : null}
                  {selectedSketchDimension && isViewportDimensionEditing ? (
                    <p className="rounded-xl border border-white/10 bg-white/5 px-3 py-2 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                      Driving length is currently being edited in the viewport.
                    </p>
                  ) : null}
                  <div className="grid grid-cols-2 gap-3">
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Start X
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={lineStartX}
                        onChange={(event) => {
                          setLineStartX(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Start Y
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={lineStartY}
                        onChange={(event) => {
                          setLineStartY(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      End X
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={lineEndX}
                        onChange={(event) => {
                          setLineEndX(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      End Y
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={lineEndY}
                        onChange={(event) => {
                          setLineEndY(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                  </div>
                  <button
                    className="cad-action-primary min-w-[140px]"
                    type="submit"
                    disabled={disabled}
                  >
                    Update Line
                  </button>
                </form>
              ) : null}
              {selectedSketchCircle ? (
                <form
                  className="space-y-3 rounded-2xl border border-white/10 bg-white/5 p-3"
                  onSubmit={(event) => {
                    event.preventDefault();
                    void onUpdateSketchCircle(
                      selectedSketchCircle.circle_id,
                      Number(circleCenterX),
                      Number(circleCenterY),
                      Number(circleRadius),
                    );
                  }}
                >
                  <p className="text-on-surface-muted">
                    Selected circle {selectedSketchCircle.circle_id} · r{" "}
                    {selectedSketchCircle.radius}
                  </p>
                  {selectedSketchDimension && !isViewportDimensionEditing ? (
                    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Driving Radius
                      <div className="mt-2 flex gap-3">
                        <input
                          className="cad-input"
                          type="number"
                          min="0.01"
                          step="0.01"
                          value={dimensionValue}
                          onChange={(event) => {
                            setDimensionValue(event.target.value);
                          }}
                          disabled={disabled}
                        />
                        <button
                          className="cad-action-primary shrink-0"
                          type="button"
                          disabled={disabled || Number(dimensionValue) <= 0}
                          onClick={() => {
                            void onUpdateSketchDimension(
                              selectedSketchDimension.dimension_id,
                              Number(dimensionValue),
                            );
                          }}
                        >
                          Drive
                        </button>
                      </div>
                    </label>
                  ) : null}
                  {selectedSketchDimension && isViewportDimensionEditing ? (
                    <p className="rounded-xl border border-white/10 bg-white/5 px-3 py-2 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                      Driving radius is currently being edited in the viewport.
                    </p>
                  ) : null}
                  <div className="grid grid-cols-3 gap-3">
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Center X
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={circleCenterX}
                        onChange={(event) => {
                          setCircleCenterX(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Center Y
                      <input
                        className="cad-input mt-2"
                        type="number"
                        step="0.01"
                        value={circleCenterY}
                        onChange={(event) => {
                          setCircleCenterY(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                    <label className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                      Radius
                      <input
                        className="cad-input mt-2"
                        type="number"
                        min="0.01"
                        step="0.01"
                        value={circleRadius}
                        onChange={(event) => {
                          setCircleRadius(event.target.value);
                        }}
                        disabled={disabled}
                      />
                    </label>
                  </div>
                  <button
                    className="cad-action-primary min-w-[140px]"
                    type="submit"
                    disabled={disabled}
                  >
                    Update Circle
                  </button>
                </form>
              ) : null}
            </>
          ) : null}
          {feature.extrude_parameters ? (
            <p className="text-on-surface-muted">
              Extrude from {feature.extrude_parameters.profile_id} · depth{" "}
              {feature.extrude_parameters.depth} mm
            </p>
          ) : null}
          <button
            className="cad-action-ghost"
            onClick={() => {
              void onRename(feature.feature_id, name || feature.name);
            }}
            disabled={disabled || name.trim().length === 0}
          >
            Rename Feature
          </button>
        </div>
      </section>
    );
  }

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="cad-kicker">Inspector</p>
          <h2 className="cad-title mt-2">Edit Selected Box</h2>
        </div>
        <button
          className="cad-action-ghost text-danger"
          onClick={() => {
            void onDelete(feature.feature_id);
          }}
          disabled={disabled}
        >
          Delete
        </button>
      </div>

      <div className="mt-5">
        <label className="text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Feature Name
          <div className="mt-2 flex gap-3">
            <input
              className="cad-input"
              value={name}
              onChange={(event) => {
                setName(event.target.value);
              }}
              disabled={disabled}
            />
            <button
              className="cad-action-ghost shrink-0"
              type="button"
              onClick={() => {
                void onRename(feature.feature_id, name);
              }}
              disabled={disabled || name.trim().length === 0}
            >
              Rename
            </button>
          </div>
        </label>
      </div>

      <form
        onSubmit={(event) => {
          event.preventDefault();
          void onSubmit(
            feature.feature_id,
            Number(width),
            Number(height),
            Number(depth),
          );
        }}
        className="mt-5 flex flex-wrap items-end gap-4"
      >
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Width
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={width}
            onChange={(event) => {
              setWidth(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Height
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={height}
            onChange={(event) => {
              setHeight(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Depth
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={depth}
            onChange={(event) => {
              setDepth(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <button
          className="cad-action-primary min-w-[140px]"
          type="submit"
          disabled={disabled}
        >
          Update Box
        </button>
      </form>
    </section>
  );
}
