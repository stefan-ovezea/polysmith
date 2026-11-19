import * as THREE from "three";
import { describe, expect, it } from "vitest";

import type { PrimitiveVisual, SolidFaceVisual } from "@/types";
import {
  applyPrimitiveVisualState,
  applySolidFaceVisualState,
  applyVertexVisualColor,
} from "./visualState";

// Regression guard: the per-face fill is a HIGHLIGHT layer, not the paint.
//
// It used to be filled opaquely with the face's imported appearance colour, and
// that material is a MeshBasicMaterial — unlit. Every imported part was
// therefore covered by flat colour cards: the 4inch enclosure (158 faces, all
// #FFFFFF) came out as one washed-out sheet with no shading at all. The colour
// is already on the body (per-vertex mesh colours plus the lit body material),
// so an idle face must be invisible.
function faceVisual(appearanceColor: string | null): SolidFaceVisual {
  return {
    fillMaterial: new THREE.MeshBasicMaterial({ transparent: true }),
    appearanceColor,
  };
}

describe("solid face fill", () => {
  it("is invisible when the face is neither hovered nor selected", () => {
    const visual = faceVisual("#FFFFFF");
    applySolidFaceVisualState(visual, { isSelected: false, isHovered: false });
    expect(visual.fillMaterial.opacity).toBe(0);
  });

  it("stays invisible for a coloured face — the body carries the colour", () => {
    // The bug: any appearance colour forced opacity 1.
    const visual = faceVisual("#868686");
    applySolidFaceVisualState(visual, { isSelected: false, isHovered: false });
    expect(visual.fillMaterial.opacity).toBe(0);
  });

  it("does not paint a hovered face, and paints a selected one clearly", () => {
    // Hover is shown by the edge highlight: a 0.08 face fill lands exactly on a
    // body-derived proxy (invisible) but sticks out of the part on a legacy
    // analytical proxy, which read as grey slabs around the leads.
    const hovered = faceVisual("#FFFFFF");
    applySolidFaceVisualState(hovered, { isSelected: false, isHovered: true });
    expect(hovered.fillMaterial.opacity).toBe(0);

    const selected = faceVisual("#FFFFFF");
    applySolidFaceVisualState(selected, { isSelected: true, isHovered: false });
    expect(selected.fillMaterial.opacity).toBeCloseTo(0.5, 5);

    // Selection wins over hover, otherwise a hovered-selected face would fade.
    const both = faceVisual("#FFFFFF");
    applySolidFaceVisualState(both, { isSelected: true, isHovered: true });
    expect(both.fillMaterial.opacity).toBeCloseTo(0.5, 5);
  });
});

describe("body vertex markers", () => {
  // They are pick targets, not model geometry: an imported enclosure carries one
  // per vertex (264 of them) and drawing them scattered a field of dots over the
  // slats, corners and posts. Idle must be invisible; hovering and selecting
  // reveal the marker so the user sees the target before clicking it.
  it("is invisible when idle", () => {
    const material = new THREE.MeshBasicMaterial({ transparent: true });
    applyVertexVisualColor(material, { isSelected: false, isHovered: false });
    expect(material.opacity).toBe(0);
  });

  it("is visible on hover and when selected", () => {
    const hovered = new THREE.MeshBasicMaterial({ transparent: true });
    applyVertexVisualColor(hovered, { isSelected: false, isHovered: true });
    expect(hovered.opacity).toBe(1);

    const selected = new THREE.MeshBasicMaterial({ transparent: true });
    applyVertexVisualColor(selected, { isSelected: true, isHovered: false });
    expect(selected.opacity).toBe(1);
  });
});

// Regression guard: an imported body's paint is its VERTEX colours, and a
// material colour multiplies them. Setting the theme's body grey on every body
// (which the visual-state pass did, overwriting the white the geometry builder
// had set) dimmed every imported surface by roughly half and washed the file's
// colours out — a black PCB with gold contacts and a white shield came back as
// one dark grey part with almost no contrast against SolidCraft's render of the
// same file.
function primitiveVisual(
  usesVertexColors: boolean,
  appearanceColor: string | null = null,
): PrimitiveVisual {
  return {
    baseMaterial: new THREE.MeshStandardMaterial({ color: 0xffffff }),
    edgeMaterial: new THREE.LineBasicMaterial(),
    appearanceColor,
    usesVertexColors,
  };
}

describe("body material tint", () => {
  const idle = { isSelected: false, isHovered: false };

  it("leaves a vertex-coloured body white, whatever the theme", () => {
    const visual = primitiveVisual(true);
    applyPrimitiveVisualState(visual, idle);
    expect(visual.baseMaterial.color.getHex()).toBe(0xffffff);
  });

  it("keeps it white when hovered or selected too", () => {
    // The highlight is the edge colour and the per-face fill; tinting the body
    // would destroy the imported paint on hover.
    const hovered = primitiveVisual(true);
    applyPrimitiveVisualState(hovered, { isSelected: false, isHovered: true });
    expect(hovered.baseMaterial.color.getHex()).toBe(0xffffff);

    const selected = primitiveVisual(true);
    applyPrimitiveVisualState(selected, { isSelected: true, isHovered: false });
    expect(selected.baseMaterial.color.getHex()).toBe(0xffffff);
    // The selection feedback still has to happen.
    expect(selected.edgeMaterial.color.getHex()).not.toBe(0xffffff);
  });

  it("still tints a modelled body with the theme body colour", () => {
    const visual = primitiveVisual(false);
    applyPrimitiveVisualState(visual, idle);
    expect(visual.baseMaterial.color.getHex()).not.toBe(0xffffff);
  });

  it("applies an explicit body colour to either kind", () => {
    const imported = primitiveVisual(true, "#ff0000");
    applyPrimitiveVisualState(imported, idle);
    expect(imported.baseMaterial.color.getHex()).toBe(0xff0000);

    const modelled = primitiveVisual(false, "#00ff00");
    applyPrimitiveVisualState(modelled, idle);
    expect(modelled.baseMaterial.color.getHex()).toBe(0x00ff00);
  });
});
