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
import {
  applyEdgeVisualColor,
  applyPrimitiveVisualState,
  applyReferencePlaneVisualState,
  applySketchProfileVisualState,
  applySolidFaceVisualState,
  applyVertexVisualColor,
  themeColor,
} from "@/utils";

interface MutableRef<T> {
  current: T;
}

type SketchEntityObject = THREE.Line | THREE.LineLoop;

interface ViewportVisualStateContext {
  primitiveVisualsRef: MutableRef<Map<string, PrimitiveVisual>>;
  primitiveStatesRef: MutableRef<Map<string, PrimitiveInteractionState>>;
  referencePlaneVisualsRef: MutableRef<Map<string, ReferencePlaneVisual>>;
  referencePlaneStatesRef: MutableRef<
    Map<string, ReferencePlaneInteractionState>
  >;
  solidFaceVisualsRef: MutableRef<Map<string, SolidFaceVisual>>;
  solidFaceStatesRef: MutableRef<Map<string, SolidFaceInteractionState>>;
  sketchProfileVisualsRef: MutableRef<Map<string, SketchProfileVisual>>;
  sketchProfileStatesRef: MutableRef<
    Map<string, SketchProfileInteractionState>
  >;
  sketchEntityObjectsRef: MutableRef<SketchEntityObject[]>;
  sketchPointObjectsRef: MutableRef<THREE.Mesh[]>;
  edgeLineObjectsRef: MutableRef<THREE.Line[]>;
  vertexObjectsRef: MutableRef<THREE.Mesh[]>;
  revealGhostEdgesRef: MutableRef<boolean>;
  dofMapRef: MutableRef<Map<string, "full" | "over">>;
  hoveredSketchEntityIdRef: MutableRef<string | null>;
  hoveredSketchPointIdRef: MutableRef<string | null>;
  hoveredEdgeIdRef: MutableRef<string | null>;
  hoveredVertexIdRef: MutableRef<string | null>;
}

export function createViewportVisualStateActions({
  primitiveVisualsRef,
  primitiveStatesRef,
  referencePlaneVisualsRef,
  referencePlaneStatesRef,
  solidFaceVisualsRef,
  solidFaceStatesRef,
  sketchProfileVisualsRef,
  sketchProfileStatesRef,
  sketchEntityObjectsRef,
  sketchPointObjectsRef,
  edgeLineObjectsRef,
  vertexObjectsRef,
  revealGhostEdgesRef,
  dofMapRef,
  hoveredSketchEntityIdRef,
  hoveredSketchPointIdRef,
  hoveredEdgeIdRef,
  hoveredVertexIdRef,
}: ViewportVisualStateContext) {
  function syncPrimitiveVisuals() {
    for (const [primitiveId, visual] of primitiveVisualsRef.current.entries()) {
      const state = primitiveStatesRef.current.get(primitiveId);
      if (!state) {
        continue;
      }

      applyPrimitiveVisualState(visual, state);
    }
  }

  function syncReferencePlaneVisuals() {
    for (const [
      referenceId,
      visual,
    ] of referencePlaneVisualsRef.current.entries()) {
      const state = referencePlaneStatesRef.current.get(referenceId);
      if (!state) {
        continue;
      }

      applyReferencePlaneVisualState(visual, state);
    }
  }

  function syncSolidFaceVisuals() {
    for (const [faceId, visual] of solidFaceVisualsRef.current.entries()) {
      const state = solidFaceStatesRef.current.get(faceId);
      if (!state) {
        continue;
      }

      applySolidFaceVisualState(visual, state);
    }
  }

  function syncSketchProfileVisuals() {
    for (const [
      profileId,
      visual,
    ] of sketchProfileVisualsRef.current.entries()) {
      const state = sketchProfileStatesRef.current.get(profileId);
      if (!state) {
        continue;
      }

      applySketchProfileVisualState(visual, state);
    }
  }

  function setHoveredFace(faceId: string | null) {
    let changed = false;

    for (const [id, state] of solidFaceStatesRef.current.entries()) {
      const nextHovered = id === faceId;
      if (state.isHovered !== nextHovered) {
        solidFaceStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSolidFaceVisuals();
    }
  }

  function setHoveredSketchProfile(profileId: string | null) {
    let changed = false;

    for (const [id, state] of sketchProfileStatesRef.current.entries()) {
      const nextHovered = id === profileId;
      if (state.isHovered !== nextHovered) {
        sketchProfileStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSketchProfileVisuals();
    }
  }

  function paintSketchEntityMaterials() {
    const dofMap = dofMapRef.current;
    for (const object of sketchEntityObjectsRef.current) {
      const id = object.userData.sketchEntityId as string | undefined;
      const isSelected = object.userData.isSelected === true;
      const isProjected = object.userData.sketchEntityIsProjected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchEntityIdRef.current;
      const material = object.material as
        | THREE.LineBasicMaterial
        | THREE.LineDashedMaterial;
      if (isSelected) {
        material.color.set(themeColor("--color-primary-edge-active", "#c3f5ff"));
      } else if (isHovered) {
        material.color.set(
          themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
        );
      } else if (isProjected) {
        material.color.set(themeColor("--cad-sketch-projected", "#ff4fd8"));
      } else if (id && dofMap.has(id)) {
        const status = dofMap.get(id)!;
        material.color.set(status === "full" ? 0x8899aa : 0xff4444);
      } else {
        material.color.set(themeColor("--color-tertiary-plane-fill", "#fff7c0"));
      }
      material.opacity = isSelected || isHovered ? 1 : 0.98;
      material.linewidth = isSelected ? 3 : isHovered ? 2.5 : 1;
    }
  }

  function setHoveredSketchEntity(entityId: string | null) {
    if (hoveredSketchEntityIdRef.current === entityId) {
      return;
    }
    hoveredSketchEntityIdRef.current = entityId;
    paintSketchEntityMaterials();
    paintDofStatusColors();
  }

  function paintDofStatusColors() {}

  function paintSketchPointMaterials() {
    for (const mesh of sketchPointObjectsRef.current) {
      const id = mesh.userData.sketchPointId as string | undefined;
      const kind = mesh.userData.sketchPointKind as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchPointIdRef.current;
      const material = mesh.material as THREE.MeshBasicMaterial;
      material.color.set(
        isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : isHovered
            ? themeColor("--color-tertiary-plane-edge-hover", "#fff2b2")
            : kind === "center" || kind === "projected" || kind === "quadrant"
              ? themeColor("--color-axis-z", "#6db4ff")
              : themeColor("--color-tertiary-plane-edge", "#ffe784"),
      );
      material.opacity = isSelected || isHovered ? 1 : 0.95;
      const scale = isSelected ? 1.35 : isHovered ? 1.25 : 1;
      mesh.scale.setScalar(scale);
    }
  }

  function setHoveredSketchPoint(pointId: string | null) {
    if (hoveredSketchPointIdRef.current === pointId) {
      return;
    }
    hoveredSketchPointIdRef.current = pointId;
    paintSketchPointMaterials();
  }

  function setHoveredPrimitive(primitiveId: string | null) {
    let changed = false;

    for (const [id, state] of primitiveStatesRef.current.entries()) {
      const nextHovered = id === primitiveId;
      if (state.isHovered !== nextHovered) {
        primitiveStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncPrimitiveVisuals();
    }
  }

  function paintEdgeMaterials(hoveredId: string | null) {
    const revealGhost = revealGhostEdgesRef.current;
    for (const line of edgeLineObjectsRef.current) {
      const id = line.userData.edgeId as string | undefined;
      const isSelected = line.userData.isSelected === true;
      const isGhost = line.userData.isGhost === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = line.material as THREE.LineBasicMaterial;
      applyEdgeVisualColor(material, {
        isSelected,
        isHovered,
        isGhost,
        revealGhost,
      });
    }
  }

  function setHoveredEdge(edgeId: string | null) {
    if (hoveredEdgeIdRef.current === edgeId) {
      return;
    }
    hoveredEdgeIdRef.current = edgeId;
    paintEdgeMaterials(edgeId);
  }

  function paintVertexMaterials(hoveredId: string | null) {
    for (const mesh of vertexObjectsRef.current) {
      const id = mesh.userData.vertexId as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = mesh.material as THREE.MeshBasicMaterial;
      applyVertexVisualColor(material, { isSelected, isHovered });
    }
  }

  function setHoveredVertex(vertexId: string | null) {
    if (hoveredVertexIdRef.current === vertexId) {
      return;
    }
    hoveredVertexIdRef.current = vertexId;
    paintVertexMaterials(vertexId);
  }

  function setHoveredReference(referenceId: string | null) {
    let changed = false;

    for (const [id, state] of referencePlaneStatesRef.current.entries()) {
      const nextHovered = id === referenceId;
      if (state.isHovered !== nextHovered) {
        referencePlaneStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncReferencePlaneVisuals();
    }
  }

  return {
    paintDofStatusColors,
    paintEdgeMaterials,
    paintSketchEntityMaterials,
    paintSketchPointMaterials,
    paintVertexMaterials,
    setHoveredEdge,
    setHoveredFace,
    setHoveredPrimitive,
    setHoveredReference,
    setHoveredSketchEntity,
    setHoveredSketchPoint,
    setHoveredSketchProfile,
    setHoveredVertex,
    syncPrimitiveVisuals,
    syncReferencePlaneVisuals,
    syncSketchProfileVisuals,
    syncSolidFaceVisuals,
  };
}
