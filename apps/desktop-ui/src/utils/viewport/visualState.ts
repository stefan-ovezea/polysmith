import * as THREE from "three";

import type {
  PrimitiveInteractionState,
  PrimitiveVisual,
  ReferencePlaneInteractionState,
  ReferencePlaneVisual,
  SketchProfileInteractionState,
  SketchProfileVisual,
  SolidFaceInteractionState,
  SolidFaceVisual,
} from "@/types";
import { themeColor } from "./themeColor";

export function applyPrimitiveVisualState(
  visual: PrimitiveVisual,
  state: PrimitiveInteractionState,
) {
  visual.baseMaterial.transparent = false;
  visual.baseMaterial.opacity = 1;
  visual.baseMaterial.emissive.set(
    themeColor("--color-cad-body-emissive", "#1a1a1c"),
  );
  visual.baseMaterial.emissiveIntensity = 0.05;

  // A body whose paint is its VERTEX COLOURS (an imported STEP part) must keep
  // a white material colour: `color` multiplies the vertex colours, and setting
  // it to the theme's body grey multiplied every imported surface by ~0.5 —
  // which is what turned a black PCB with gold contacts and a white shield into
  // one dark grey part with no contrast. The colour is only applied when the
  // body carries no vertex paint, or when the user explicitly recoloured it.
  const bodyColor =
    visual.appearanceColor ??
    (visual.usesVertexColors
      ? null
      : themeColor("--color-cad-body", "#b8b8b8"));
  const paint = (color: string | null) => {
    if (color !== null) {
      visual.baseMaterial.color.set(color);
    }
  };

  if (state.isSelected) {
    paint(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-selected", "#ff9a3c"),
    );
    return;
  }

  if (state.isHovered) {
    paint(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-hover", "#3da9ff"),
    );
    return;
  }

  paint(bodyColor);
  visual.edgeMaterial.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
}

export function applyEdgeVisualColor(
  material: THREE.LineBasicMaterial,
  state: {
    isSelected: boolean;
    isHovered: boolean;
    isGhost?: boolean;
    revealGhost?: boolean;
  },
) {
  if (state.isSelected) {
    material.color.set(themeColor("--color-cad-edge-selected", "#ff9a3c"));
    material.opacity = 1;
    return;
  }
  if (state.isHovered) {
    material.color.set(themeColor("--color-cad-edge-hover", "#3da9ff"));
    material.opacity = 1;
    return;
  }
  if (state.isGhost && !state.revealGhost) {
    material.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
    material.opacity = 0.001;
    return;
  }
  material.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
  material.opacity = 0.85;
}

export function applyVertexVisualColor(
  material: THREE.MeshBasicMaterial,
  state: { isSelected: boolean; isHovered: boolean },
) {
  if (state.isSelected) {
    material.color.set(themeColor("--color-cad-vertex-selected", "#ff9a3c"));
    material.opacity = 1;
    return;
  }
  if (state.isHovered) {
    material.color.set(themeColor("--color-cad-vertex-hover", "#3da9ff"));
    material.opacity = 1;
    return;
  }
  // An idle vertex marker is INVISIBLE, not merely dark.
  //
  // Every body vertex carries one of these small spheres so it can be picked —
  // they are pick targets, not part of the model. Drawing them (opacity 0.95)
  // scattered a field of dots over the part: an imported enclosure showed 264 of
  // them across its slats, corners and posts, which reads as dirt on the model
  // and hides the very edges it sits on. Hovering still reveals the marker
  // (blue) and selecting keeps it (orange), so the pick target is visible
  // exactly when a click is about to use it.
  material.color.set(themeColor("--color-cad-vertex", "#1c1c1e"));
  material.opacity = 0;
}

export function applyReferencePlaneVisualState(
  visual: ReferencePlaneVisual,
  state: ReferencePlaneInteractionState,
) {
  if (state.isActiveSketchPlane) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-selected", "#f7e38a"),
    );
    visual.fillMaterial.opacity = 0.38;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-active", "#fff4b8"),
    );
    return;
  }

  if (state.isSelected) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-selected", "#f7e38a"),
    );
    visual.fillMaterial.opacity = 0.34;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-selected", "#ffe99a"),
    );
    return;
  }

  if (state.isHovered) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-hover", "#fff0aa"),
    );
    visual.fillMaterial.opacity = 0.3;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
    );
    return;
  }

  visual.fillMaterial.color.set(
    themeColor("--color-tertiary-plane-fill", "#fff7c0"),
  );
  visual.fillMaterial.opacity = 0.24;
  visual.edgeMaterial.color.set(
    themeColor("--color-tertiary-plane-edge", "#ffe784"),
  );
}

export function applySolidFaceVisualState(
  visual: SolidFaceVisual,
  state: SolidFaceInteractionState,
) {
  // The face fill is a HIGHLIGHT layer, never the paint.
  //
  // It used to be filled opaquely with the face's imported appearance colour.
  // That colour is already the body's: the STEP styles are baked into the body
  // mesh's per-vertex colours and into the body material (see
  // applyPrimitiveVisualState), both of which are LIT. The fill is a
  // MeshBasicMaterial — unlit — so an opaque fill replaced every shaded pixel
  // with a flat card: a single-colour part such as the 4inch enclosure (158
  // faces, all #FFFFFF) rendered as one washed-out sheet with no shading at
  // all, while thousands of tiny PCB faces hid the same defect behind their
  // colour variety. Painting a face only when it is hovered or selected keeps
  // the colour and brings the shading back.
  if (state.isSelected) {
    visual.fillMaterial.color.set(
      themeColor("--color-primary-bright", "#4be7ff"),
    );
    visual.fillMaterial.opacity = 0.5;
    return;
  }

  if (state.isHovered) {
    visual.fillMaterial.color.set(
      themeColor("--cad-face-hover-fill", "#ffffff"),
    );
    visual.fillMaterial.opacity = 0.08;
    return;
  }

  visual.fillMaterial.color.set(themeColor("--cad-face-hover-fill", "#ffffff"));
  visual.fillMaterial.opacity = 0;
}

export function applySketchProfileVisualState(
  visual: SketchProfileVisual,
  state: SketchProfileInteractionState,
) {
  if (state.isHovered || state.isSelected) {
    visual.fillMaterial.color.set(
      state.isSelected
        ? themeColor("--color-primary-bright", "#4be7ff")
        : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    );
    visual.fillMaterial.opacity = state.isSelected ? 0.5 : 0.18;
    for (const material of visual.edgeMaterials) {
      material.color.set(
        state.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
      );
      material.opacity = 0.98;
      material.linewidth = state.isSelected ? 3 : 2.5;
    }
    // Inner-loop (hole) outlines stay neutral: highlighting them makes
    // it look like the contained circle profile is hovered/selected too.
    for (const material of visual.holeEdgeMaterials) {
      material.opacity = 0;
    }
    return;
  }

  visual.fillMaterial.opacity = 0;
  for (const material of visual.edgeMaterials) {
    material.opacity = 0;
    material.linewidth = 1;
  }
}
