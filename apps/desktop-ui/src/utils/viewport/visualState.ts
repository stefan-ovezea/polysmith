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

  const bodyColor =
    visual.appearanceColor ?? themeColor("--color-cad-body", "#b8b8b8");

  if (state.isSelected) {
    visual.baseMaterial.color.set(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-selected", "#ff9a3c"),
    );
    return;
  }

  if (state.isHovered) {
    visual.baseMaterial.color.set(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-hover", "#3da9ff"),
    );
    return;
  }

  visual.baseMaterial.color.set(bodyColor);
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
  material.color.set(themeColor("--color-cad-vertex", "#1c1c1e"));
  material.opacity = 0.95;
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
  if (visual.appearanceColor) {
    visual.fillMaterial.color.set(visual.appearanceColor);
    visual.fillMaterial.opacity = 1;
    return;
  }

  if (state.isSelected) {
    visual.fillMaterial.color.set(
      themeColor("--color-primary-soft", "#c3f5ff"),
    );
    visual.fillMaterial.opacity = 0.24;
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
        ? themeColor("--color-primary-soft", "#c3f5ff")
        : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    );
    visual.fillMaterial.opacity = state.isSelected ? 0.24 : 0.18;
    for (const material of visual.edgeMaterials) {
      material.color.set(
        state.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
      );
      material.opacity = 0.98;
      material.linewidth = state.isSelected ? 3 : 2.5;
    }
    return;
  }

  visual.fillMaterial.opacity = 0;
  for (const material of visual.edgeMaterials) {
    material.opacity = 0;
    material.linewidth = 1;
  }
}
