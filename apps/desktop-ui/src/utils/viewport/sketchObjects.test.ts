// @vitest-environment node
// Reference-line rendering rules. The document owns the geometry; the
// renderer owns these two presentation rules, and getting them wrong is
// visible immediately (a centerline indistinguishable from construction, or
// an "infinite" line that stops at its endpoints).
import * as THREE from "three";
import { describe, expect, it } from "vitest";

import type { SketchLineScene } from "@/types";

import { buildSketchLineObject } from "./sketchObjects";

function lineScene(
  overrides: Partial<SketchLineScene> = {},
): SketchLineScene {
  return {
    isPreview: false,
    lineId: "line-1",
    startPointId: "vertex-1",
    endPointId: "vertex-2",
    planeId: "ref-plane-xy",
    start: [0, 0, 0],
    end: [10, 0, 0],
    isSelected: false,
    constraint: null,
    isConstruction: false,
    lineType: null,
    isProjected: false,
    generatedBy: null,
    ...overrides,
  };
}

function endpoints(object: THREE.Line) {
  const positions = object.geometry.getAttribute("position");
  return [
    positions.getX(0),
    positions.getY(0),
    positions.getZ(0),
    positions.getX(1),
    positions.getY(1),
    positions.getZ(1),
  ];
}

describe("buildSketchLineObject reference lines", () => {
  it("renders an infinite line far past its endpoints", () => {
    const finite = buildSketchLineObject(
      lineScene({ isConstruction: true, lineType: null }),
    ) as THREE.Line;
    const infinite = buildSketchLineObject(
      lineScene({ isConstruction: true, lineType: "infinite" }),
    ) as THREE.Line;

    const [fsx] = endpoints(finite);
    const [isx, , , iex] = endpoints(infinite);
    expect(isx).toBeLessThan(fsx);
    expect(Math.abs(iex)).toBeGreaterThan(1000);
  });

  it("gives a centerline a longer dash than a plain construction line", () => {
    const construction = buildSketchLineObject(
      lineScene({ isConstruction: true, lineType: null }),
    ) as THREE.Line;
    const centerline = buildSketchLineObject(
      lineScene({ isConstruction: true, lineType: "centerline" }),
    ) as THREE.Line;

    const constructionMaterial =
      construction.material as THREE.LineDashedMaterial;
    const centerlineMaterial = centerline.material as THREE.LineDashedMaterial;
    expect(centerlineMaterial).toBeInstanceOf(THREE.LineDashedMaterial);
    expect(centerlineMaterial.dashSize).toBeGreaterThan(
      constructionMaterial.dashSize,
    );
  });

  it("keeps a solid line solid", () => {
    const solid = buildSketchLineObject(lineScene()) as THREE.Line;
    expect(solid.material).toBeInstanceOf(THREE.LineBasicMaterial);
  });
});
