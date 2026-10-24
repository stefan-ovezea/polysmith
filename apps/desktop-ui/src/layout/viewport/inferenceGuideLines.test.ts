// @vitest-environment node
// Inference guides (the dotted "aligned with that vertex" hint shown while
// drawing a line). Getting these wrong is visible immediately: too faint or
// depth-tested against the sketch plane and they read as a flicker rather than a
// hint.
import * as THREE from "three";
import { describe, expect, it } from "vitest";

import { buildInferenceGuideLines } from "./draftLinePreview";

describe("inference guide lines", () => {
  const guides = [
    { from: [0, 0] as [number, number], draft: [10, 0] as [number, number] },
    { from: [5, 5] as [number, number], draft: [5, 0] as [number, number] },
  ];

  it("builds one dashed line per guide, sharing one material", () => {
    const lines = buildInferenceGuideLines({
      guides,
      planeId: "ref-plane-xy",
      planeFrame: null,
    });
    expect(lines).toHaveLength(2);
    // A material per line meant the cleanup disposed the same material once per
    // guide; the batch is one visual element.
    expect(lines[0].material).toBe(lines[1].material);
  });

  it("is dashed and readable, not a faint line", () => {
    const [line] = buildInferenceGuideLines({
      guides,
      planeId: "ref-plane-xy",
      planeFrame: null,
    });
    const material = line.material as THREE.LineDashedMaterial;
    expect(material.isLineDashedMaterial).toBe(true);
    // 0.42 was too faint against the plane on a dark theme.
    expect(material.opacity).toBeGreaterThanOrEqual(0.8);
    // Dashes need the line-distance attribute or they render solid.
    expect(
      line.geometry.getAttribute("lineDistance")?.count,
    ).toBeGreaterThan(0);
  });

  it("draws over the sketch plane instead of z-fighting with it", () => {
    const [line] = buildInferenceGuideLines({
      guides,
      planeId: "ref-plane-xy",
      planeFrame: null,
    });
    const material = line.material as THREE.LineDashedMaterial;
    // The guide lies exactly on the plane; depth testing it there is a z-fight.
    expect(material.depthTest).toBe(false);
    // Sketch lines draw at 7, so the hint must come after them.
    expect(line.renderOrder).toBeGreaterThan(7);
  });
});
