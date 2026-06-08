import * as THREE from "three";

import type {
  CutPreviewScene,
  PrimitiveInteractionState,
  PrimitiveVisual,
  ReferencePlaneInteractionState,
  ReferencePlaneVisual,
  SceneEdge,
  ScenePrimitive,
  SceneReference,
  SolidFaceInteractionState,
  SolidFaceScene,
  SolidFaceVisual,
} from "@/types";
import {
  buildCutPreviewObject,
  buildPrimitiveObject,
  buildReferenceAxisObject,
  buildReferenceHelixObject,
  buildReferencePlaneObject,
  buildReferencePointObject,
  buildSceneEdgeObject,
  buildSolidFaceObject,
} from "@/utils";

export function addPrimitiveSceneObjects({
  primitives,
  contentGroup,
  meshes,
  primitiveVisuals,
  primitiveStates,
}: {
  primitives: readonly ScenePrimitive[];
  contentGroup: THREE.Group;
  meshes: THREE.Mesh[];
  primitiveVisuals: Map<string, PrimitiveVisual>;
  primitiveStates: Map<string, PrimitiveInteractionState>;
}) {
  for (const primitive of primitives) {
    const object = buildPrimitiveObject(primitive);
    meshes.push(object.mesh);
    primitiveVisuals.set(primitive.primitiveId, object.visual);
    primitiveStates.set(primitive.primitiveId, {
      isSelected: primitive.isSelected,
      isHovered: false,
    });
    contentGroup.add(object.mesh);
    contentGroup.add(object.edges);
  }
}

export function addReferenceSceneObjects({
  references,
  referenceGroup,
  showReferencePlanes,
  referencePlaneMeshes,
  referencePlaneVisuals,
  referencePlaneStates,
}: {
  references: readonly SceneReference[];
  referenceGroup: THREE.Group;
  showReferencePlanes: boolean;
  referencePlaneMeshes: THREE.Mesh[];
  referencePlaneVisuals: Map<string, ReferencePlaneVisual>;
  referencePlaneStates: Map<string, ReferencePlaneInteractionState>;
}) {
  for (const reference of references) {
    if (reference.kind === "reference_plane") {
      if (!showReferencePlanes) {
        continue;
      }

      const object = buildReferencePlaneObject(reference);
      referencePlaneMeshes.push(object.mesh);
      referencePlaneVisuals.set(reference.referenceId, object.visual);
      referencePlaneStates.set(reference.referenceId, {
        isSelected: reference.isSelected,
        isHovered: false,
        isActiveSketchPlane: reference.isActiveSketchPlane,
      });
      referenceGroup.add(object.mesh);
      referenceGroup.add(object.edges);
      continue;
    }

    if (reference.kind === "reference_axis") {
      const axisObject = buildReferenceAxisObject(reference);
      referenceGroup.add(axisObject.line);
    } else if (reference.kind === "reference_point") {
      const pointObject = buildReferencePointObject(reference);
      referenceGroup.add(pointObject.mesh);
    } else if (reference.kind === "reference_helix") {
      const helixObject = buildReferenceHelixObject(reference);
      referenceGroup.add(helixObject.line);
    }
  }
}

export function addSolidSceneObjects({
  faces,
  edges,
  cutPreviews,
  contentGroup,
  faceMeshes,
  solidFaceVisuals,
  solidFaceStates,
  edgeLineObjects,
  cutPreviewObjects,
}: {
  faces: readonly SolidFaceScene[];
  edges: readonly SceneEdge[];
  cutPreviews: readonly CutPreviewScene[];
  contentGroup: THREE.Group;
  faceMeshes: THREE.Mesh[];
  solidFaceVisuals: Map<string, SolidFaceVisual>;
  solidFaceStates: Map<string, SolidFaceInteractionState>;
  edgeLineObjects: THREE.Line[];
  cutPreviewObjects: THREE.Mesh[];
}) {
  for (const face of faces) {
    const faceObject = buildSolidFaceObject(face);
    faceMeshes.push(faceObject.mesh);
    solidFaceVisuals.set(face.faceId, faceObject.visual);
    solidFaceStates.set(face.faceId, {
      isSelected: face.isSelected,
      isHovered: false,
    });
    contentGroup.add(faceObject.mesh);
  }

  for (const edge of edges) {
    const edgeLine = buildSceneEdgeObject(edge);
    edgeLineObjects.push(edgeLine);
    contentGroup.add(edgeLine);
  }

  for (const preview of cutPreviews) {
    const cutPreviewMesh = buildCutPreviewObject(preview);
    cutPreviewObjects.push(cutPreviewMesh);
    contentGroup.add(cutPreviewMesh);
  }
}
