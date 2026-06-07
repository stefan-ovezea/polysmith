import * as THREE from "three";

import type { ViewportScene } from "@/types";
import {
  handleSceneSelectionHit,
  handleSharedSketchSelectionHit,
  type SharedSketchSelectionHit,
} from "./sketchClickSelection";
import { pickVisibleSketchLineScreenSpace } from "./sceneTargetPicking";

interface PointerUpSceneSelectionParams {
  event: PointerEvent;
  sceneData: ViewportScene | null;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
  inactiveSketchEntityPickEnabled: boolean;
  pickInactiveSketchLine:
    | ((sketchLineId: string) => void | Promise<void>)
    | null
    | undefined;
  intersectSceneTargets: (event: PointerEvent) => SharedSketchSelectionHit;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  selectReference: (referenceId: string) => Promise<void>;
  selectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
  selectPrimitive: (primitiveId: string) => Promise<void>;
}

export function handlePointerUpSceneSelection({
  event,
  sceneData,
  camera,
  renderer,
  inactiveSketchEntityPickEnabled,
  pickInactiveSketchLine,
  intersectSceneTargets,
  selectSketchEntity,
  selectSketchProfile,
  selectReference,
  selectVertex,
  selectEdge,
  selectFace,
  selectPrimitive,
}: PointerUpSceneSelectionParams) {
  if (inactiveSketchEntityPickEnabled && pickInactiveSketchLine) {
    const sketchLineId = pickVisibleSketchLineScreenSpace({
      event,
      sceneData,
      camera,
      renderer,
      maxDistancePx: 16,
    });
    if (sketchLineId) {
      void pickInactiveSketchLine(sketchLineId);
      return true;
    }
  }

  const additiveSelection = event.shiftKey || event.ctrlKey || event.metaKey;
  const hit = intersectSceneTargets(event);
  if (
    handleSharedSketchSelectionHit({
      hit,
      inactiveSketchEntityPickEnabled,
      additiveSelection,
      selectSketchEntity,
      selectSketchProfile,
    })
  ) {
    return true;
  }

  return handleSceneSelectionHit({
    hit,
    additiveSelection,
    selectReference,
    selectVertex,
    selectEdge,
    selectFace,
    selectPrimitive,
  });
}
