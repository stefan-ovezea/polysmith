// @vitest-environment node
// S1: the pick dispatcher honours the armed dimension mode.
//
// The bug this pins: in Radius / Diameter / Arc length mode a click on a LINE
// fell through to the auto path, which started a linear placement — the mode
// promised a circular dimension and produced a linear one. Each case below
// asserts both the positive (the right command fires) and the negative (the
// auto path stays out of it).
import { describe, expect, it, vi } from "vitest";

import { handleDimensionToolClick } from "./dimensionToolPicking";
import type { DimensionToolMode } from "@/types";

function makeContext(
  dimensionToolMode: DimensionToolMode,
  hit:
    | { kind: "sketch_entity"; id: string; entityKind: string; isProjected: boolean }
    | { kind: "sketch_point"; id: string }
    | { kind: "sketch_dimension"; id: string }
    | null,
) {
  return {
    hit,
    sketch: null,
    pendingPlacement: false,
    pendingSourceId: null,
    pendingDimensionId: null,
    getFirstEntityId: () => null,
    getFirstPoint: () => null,
    clearFirstPick: vi.fn(),
    clearFirstEntity: vi.fn(),
    clearPendingPlacement: vi.fn(),
    stageFirstEntity: vi.fn(),
    stageFirstPoint: vi.fn(),
    deleteSketchDimension: vi.fn(),
    handleDimensionClick: vi.fn(),
    createAngleOrDistance: vi.fn(),
    createVertexDistance: vi.fn(),
    createLine: vi.fn(),
    createLineAngle: vi.fn(),
    createCircle: vi.fn(),
    selectCircle: vi.fn(),
    createArc: vi.fn(),
    createArcLength: vi.fn(),
    selectArc: vi.fn(),
    createPolygon: vi.fn(),
    selectPolygon: vi.fn(),
    selectLine: vi.fn(),
    startLinearPlacement: vi.fn(),
    dimensionToolMode,
    cursorLocal: [1, 1] as [number, number],
  };
}

const entity = (id: string, entityKind: string) => ({
  kind: "sketch_entity" as const,
  id,
  entityKind,
  isProjected: false,
});

describe("dimension mode filtering", () => {
  it("radius: a circle gets a radius dimension", () => {
    const context = makeContext("radius", entity("circle-1", "circle"));
    handleDimensionToolClick(context);
    expect(context.createCircle).toHaveBeenCalledWith("circle-1", "");
  });

  it("radius: a LINE no longer produces a linear dimension", () => {
    const context = makeContext("radius", entity("line-1", "line"));
    expect(handleDimensionToolClick(context)).toBe(true);
    expect(context.createLine).not.toHaveBeenCalled();
    expect(context.startLinearPlacement).not.toHaveBeenCalled();
    expect(context.createVertexDistance).not.toHaveBeenCalled();
  });

  it("diameter: an arc gets an arc dimension, a line does nothing", () => {
    const arcContext = makeContext("diameter", entity("arc-1", "arc"));
    handleDimensionToolClick(arcContext);
    expect(arcContext.createArc).toHaveBeenCalledWith("arc-1");

    const lineContext = makeContext("diameter", entity("line-1", "line"));
    handleDimensionToolClick(lineContext);
    expect(lineContext.startLinearPlacement).not.toHaveBeenCalled();
    expect(lineContext.createLine).not.toHaveBeenCalled();
  });

  it("arc length: an arc gets the arc-length dimension, a line does nothing", () => {
    const arcContext = makeContext("arc_length", entity("arc-1", "arc"));
    handleDimensionToolClick(arcContext);
    expect(arcContext.createArcLength).toHaveBeenCalledWith("arc-1");

    const lineContext = makeContext("arc_length", entity("line-1", "line"));
    handleDimensionToolClick(lineContext);
    expect(lineContext.startLinearPlacement).not.toHaveBeenCalled();
    expect(lineContext.createLine).not.toHaveBeenCalled();
  });

  it("angular: a circle is not a valid angle target", () => {
    const context = makeContext("angular", entity("circle-1", "circle"));
    handleDimensionToolClick(context);
    expect(context.createAngleOrDistance).not.toHaveBeenCalled();
    expect(context.createLineAngle).not.toHaveBeenCalled();
  });

  it("auto: clicking a line still starts the drag-to-choose placement", () => {
    const context = makeContext("auto", entity("line-1", "line"));
    handleDimensionToolClick(context);
    expect(context.startLinearPlacement).toHaveBeenCalledWith("line-1");
  });
});
