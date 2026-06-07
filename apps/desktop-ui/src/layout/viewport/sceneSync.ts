import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type {
  DocumentState,
  PrimitiveInteractionState,
  PrimitiveVisual,
  ReferencePlaneInteractionState,
  ReferencePlaneVisual,
  SketchDimensionScene,
  SketchPlaneFrame,
  SketchProfileInteractionState,
  SketchProfileVisual,
  SolidFaceInteractionState,
  SolidFaceVisual,
  ViewportScene,
  ViewportState,
} from "@/types";
import { disposeGroup, frameCamera } from "@/utils";
import { updateEndpointDragSceneObjects } from "./sceneIncrementalUpdate";
import {
  addCamSceneObjects,
  addCamToolpathLines,
} from "./camSceneObjects";
import {
  addPrimitiveSceneObjects,
  addReferenceSceneObjects,
  addSolidSceneObjects,
} from "./sceneObjectPopulation";
import { addSketchSceneObjects } from "./sketchSceneObjects";
import { buildMoveGizmoObject, type MoveGizmoDescriptor } from "./moveGizmo";
import type { SelectedConstraintState } from "./contextMenuState";
import type { EndpointDrag } from "./endpointDrag";

interface MutableRef<T> {
  current: T;
}

interface ViewportSceneGroups {
  scene: THREE.Scene | null;
  camera: THREE.OrthographicCamera | null;
  controls: OrbitControls | null;
  contentGroup: THREE.Group | null;
  referenceGroup: THREE.Group | null;
  sketchGroup: THREE.Group | null;
}

interface ReadyViewportSceneGroups {
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  contentGroup: THREE.Group;
  referenceGroup: THREE.Group;
  sketchGroup: THREE.Group;
}

interface ViewportSceneSyncRefs {
  pendingEndpointCommit: MutableRef<boolean>;
  endpointDrag: MutableRef<EndpointDrag | null>;
  activeSketchPlaneFrame: MutableRef<SketchPlaneFrame | null>;
  sketchEntityObjectById: MutableRef<Map<string, THREE.Line | THREE.LineLoop>>;
  sketchPointObjectById: MutableRef<Map<string, THREE.Mesh>>;
  sketchConstraintObjects: MutableRef<THREE.Object3D[]>;
  dragCursor: MutableRef<{ x: number; y: number } | null>;
  lastGeometryKey: MutableRef<string>;
  hoveredEdgeId: MutableRef<string | null>;
  hoveredVertexId: MutableRef<string | null>;
  hoveredSketchEntityId: MutableRef<string | null>;
  hoveredSketchPointId: MutableRef<string | null>;
  meshes: MutableRef<THREE.Mesh[]>;
  primitiveVisuals: MutableRef<Map<string, PrimitiveVisual>>;
  primitiveStates: MutableRef<Map<string, PrimitiveInteractionState>>;
  referencePlaneMeshes: MutableRef<THREE.Mesh[]>;
  referencePlaneVisuals: MutableRef<Map<string, ReferencePlaneVisual>>;
  referencePlaneStates: MutableRef<Map<string, ReferencePlaneInteractionState>>;
  faceMeshes: MutableRef<THREE.Mesh[]>;
  solidFaceVisuals: MutableRef<Map<string, SolidFaceVisual>>;
  solidFaceStates: MutableRef<Map<string, SolidFaceInteractionState>>;
  edgeLineObjects: MutableRef<THREE.Line[]>;
  cutPreviewObjects: MutableRef<THREE.Mesh[]>;
  toolpathLines: MutableRef<THREE.Line[]>;
  moveGizmoObjects: MutableRef<THREE.Object3D[]>;
  hiddenRelationPreviewDimensionIds: MutableRef<Set<string>>;
  selectedConstraint: MutableRef<SelectedConstraintState | null>;
  sketchEntityObjects: MutableRef<Array<THREE.Line | THREE.LineLoop>>;
  sketchDimensionObjects: MutableRef<THREE.Object3D[]>;
  sketchProfileObjects: MutableRef<THREE.Group[]>;
  sketchProfileVisuals: MutableRef<Map<string, SketchProfileVisual>>;
  sketchProfileStates: MutableRef<Map<string, SketchProfileInteractionState>>;
  sketchPointObjects: MutableRef<THREE.Mesh[]>;
}

interface SyncViewportSceneParams {
  groups: ViewportSceneGroups;
  refs: ViewportSceneSyncRefs;
  sceneData: ViewportScene | null;
  document: DocumentState | null;
  viewport: ViewportState | null;
  displayedSketchDimensions: readonly SketchDimensionScene[];
  displayUnits: "mm" | "in";
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  showReferencePlanes: boolean;
  showStock: boolean;
  wcsOrientation: string;
  moveGizmo: MoveGizmoDescriptor | null | undefined;
  clearViewportSceneObjectRefs: () => void;
  clearDragPreviewLines: () => void;
  setConstraintPreview: (preview: null) => void;
  syncPrimitiveVisuals: () => void;
  syncReferencePlaneVisuals: () => void;
  syncSolidFaceVisuals: () => void;
  syncSketchProfileVisuals: () => void;
  paintSketchEntityMaterials: () => void;
  paintSketchPointMaterials: () => void;
  paintDofStatusColors: () => void;
}

export function syncViewportScene(params: SyncViewportSceneParams) {
  const groups = readyViewportSceneGroups(params.groups);
  if (!groups) {
    return;
  }

  const hadPendingCommit = params.refs.pendingEndpointCommit.current;
  params.refs.pendingEndpointCommit.current = false;

  if (
    syncEndpointDragScene({
      refs: params.refs,
      sceneData: params.sceneData,
      sketchGroup: groups.sketchGroup,
      hadPendingCommit,
      clearDragPreviewLines: params.clearDragPreviewLines,
      setConstraintPreview: params.setConstraintPreview,
    })
  ) {
    return;
  }

  rebuildViewportScene(params, groups);

  releaseEndpointDragPreview({
    refs: params.refs,
    hadPendingCommit,
    clearDragPreviewLines: params.clearDragPreviewLines,
    setConstraintPreview: params.setConstraintPreview,
  });

  syncSceneVisuals(params);

  updateInitialCameraFrame({
    sceneData: params.sceneData,
    activeSketchPlaneId: params.activeSketchPlaneId,
    camera: groups.camera,
    controls: groups.controls,
    lastGeometryKeyRef: params.refs.lastGeometryKey,
  });
}

function readyViewportSceneGroups({
  scene,
  camera,
  controls,
  contentGroup,
  referenceGroup,
  sketchGroup,
}: ViewportSceneGroups): ReadyViewportSceneGroups | null {
  if (
    !scene ||
    !camera ||
    !controls ||
    !contentGroup ||
    !referenceGroup ||
    !sketchGroup
  ) {
    return null;
  }
  return { scene, camera, controls, contentGroup, referenceGroup, sketchGroup };
}

function syncEndpointDragScene({
  refs,
  sceneData,
  sketchGroup,
  hadPendingCommit,
  clearDragPreviewLines,
  setConstraintPreview,
}: {
  refs: ViewportSceneSyncRefs;
  sceneData: ViewportScene | null;
  sketchGroup: THREE.Group;
  hadPendingCommit: boolean;
  clearDragPreviewLines: () => void;
  setConstraintPreview: (preview: null) => void;
}) {
  const isDragging = refs.endpointDrag.current !== null && !hadPendingCommit;
  if (!isDragging || !sceneData || sketchGroup.children.length === 0) {
    return false;
  }

  updateEndpointDragSceneObjects({
    sceneData,
    planeFrame: refs.activeSketchPlaneFrame.current,
    sketchEntityObjectById: refs.sketchEntityObjectById.current,
    sketchPointObjectById: refs.sketchPointObjectById.current,
    sketchConstraintObjects: refs.sketchConstraintObjects.current,
  });

  releaseEndpointDragPreview({
    refs,
    hadPendingCommit,
    clearDragPreviewLines,
    setConstraintPreview,
  });
  refs.lastGeometryKey.current = sceneData.geometryKey;
  return true;
}

function rebuildViewportScene(
  params: SyncViewportSceneParams,
  groups: ReadyViewportSceneGroups,
) {
  resetViewportSceneGroups(params, groups);

  if (!params.sceneData) {
    params.refs.lastGeometryKey.current = "";
    return;
  }

  addModelSceneObjects(params, groups);
  addMoveGizmoSceneObject(params, groups.contentGroup);
  addSketchSceneObjectSet(params, groups.sketchGroup);
}

function resetViewportSceneGroups(
  { refs, clearViewportSceneObjectRefs }: SyncViewportSceneParams,
  { contentGroup, referenceGroup, sketchGroup }: ReadyViewportSceneGroups,
) {
  disposeGroup(contentGroup);
  disposeGroup(referenceGroup);
  disposeGroup(sketchGroup);
  clearViewportSceneObjectRefs();
  clearHoverRefs(refs);
}

function addModelSceneObjects(
  {
    refs,
    sceneData,
    document,
    viewport,
    showReferencePlanes,
    showStock,
    wcsOrientation,
  }: SyncViewportSceneParams,
  { contentGroup, referenceGroup }: ReadyViewportSceneGroups,
) {
  if (!sceneData) {
    return;
  }

  addPrimitiveSceneObjects({
    primitives: sceneData.primitives,
    contentGroup,
    meshes: refs.meshes.current,
    primitiveVisuals: refs.primitiveVisuals.current,
    primitiveStates: refs.primitiveStates.current,
  });

  addReferenceSceneObjects({
    references: sceneData.references,
    referenceGroup,
    showReferencePlanes,
    referencePlaneMeshes: refs.referencePlaneMeshes.current,
    referencePlaneVisuals: refs.referencePlaneVisuals.current,
    referencePlaneStates: refs.referencePlaneStates.current,
  });

  addCamSceneObjects({
    document,
    viewport,
    referenceGroup,
    showStock,
    wcsOrientation,
  });

  addSolidSceneObjects({
    faces: sceneData.solidFaces,
    edges: sceneData.edges,
    cutPreviews: sceneData.cutPreviews,
    contentGroup,
    faceMeshes: refs.faceMeshes.current,
    solidFaceVisuals: refs.solidFaceVisuals.current,
    solidFaceStates: refs.solidFaceStates.current,
    edgeLineObjects: refs.edgeLineObjects.current,
    cutPreviewObjects: refs.cutPreviewObjects.current,
  });

  refs.toolpathLines.current = addCamToolpathLines({
    viewport,
    contentGroup,
  });
}

function addMoveGizmoSceneObject(
  { refs, moveGizmo }: SyncViewportSceneParams,
  contentGroup: THREE.Group,
) {
  if (moveGizmo && !moveGizmo.disabled) {
    const object = buildMoveGizmoObject(moveGizmo);
    refs.moveGizmoObjects.current = object.pickables;
    contentGroup.add(object.group);
  }
}

function addSketchSceneObjectSet(
  {
    refs,
    sceneData,
    displayedSketchDimensions,
    displayUnits,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  }: SyncViewportSceneParams,
  sketchGroup: THREE.Group,
) {
  if (!sceneData) {
    return;
  }

  addSketchSceneObjects({
    sceneData,
    displayedSketchDimensions,
    displayUnits,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
    hiddenRelationPreviewDimensionIds:
      refs.hiddenRelationPreviewDimensionIds.current,
    selectedConstraint: refs.selectedConstraint.current,
    sketchGroup,
    sketchEntityObjects: refs.sketchEntityObjects.current,
    sketchEntityObjectById: refs.sketchEntityObjectById.current,
    sketchDimensionObjects: refs.sketchDimensionObjects.current,
    sketchConstraintObjects: refs.sketchConstraintObjects.current,
    sketchProfileObjects: refs.sketchProfileObjects.current,
    sketchProfileVisuals: refs.sketchProfileVisuals.current,
    sketchProfileStates: refs.sketchProfileStates.current,
    sketchPointObjects: refs.sketchPointObjects.current,
    sketchPointObjectById: refs.sketchPointObjectById.current,
  });
}

function syncSceneVisuals({
  syncPrimitiveVisuals,
  syncReferencePlaneVisuals,
  syncSolidFaceVisuals,
  syncSketchProfileVisuals,
  paintSketchEntityMaterials,
  paintSketchPointMaterials,
  paintDofStatusColors,
}: SyncViewportSceneParams) {
  syncPrimitiveVisuals();
  syncReferencePlaneVisuals();
  syncSolidFaceVisuals();
  syncSketchProfileVisuals();
  paintSketchEntityMaterials();
  paintSketchPointMaterials();
  paintDofStatusColors();
}

function clearHoverRefs(refs: ViewportSceneSyncRefs) {
  refs.hoveredEdgeId.current = null;
  refs.hoveredVertexId.current = null;
  refs.hoveredSketchEntityId.current = null;
  refs.hoveredSketchPointId.current = null;
}

function releaseEndpointDragPreview({
  refs,
  hadPendingCommit,
  clearDragPreviewLines,
  setConstraintPreview,
}: {
  refs: ViewportSceneSyncRefs;
  hadPendingCommit: boolean;
  clearDragPreviewLines: () => void;
  setConstraintPreview: (preview: null) => void;
}) {
  if (!hadPendingCommit) {
    return;
  }
  refs.endpointDrag.current = null;
  clearDragPreviewLines();
  setConstraintPreview(null);
  refs.dragCursor.current = null;
}

function updateInitialCameraFrame({
  sceneData,
  activeSketchPlaneId,
  camera,
  controls,
  lastGeometryKeyRef,
}: {
  sceneData: ViewportScene | null;
  activeSketchPlaneId: string | null;
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  lastGeometryKeyRef: MutableRef<string>;
}) {
  if (!sceneData || sceneData.geometryKey === lastGeometryKeyRef.current) {
    return;
  }

  const isFirstSceneLoad = lastGeometryKeyRef.current === "";
  if (isFirstSceneLoad && !activeSketchPlaneId) {
    frameCamera(
      camera,
      controls,
      sceneData.bounds.center,
      sceneData.bounds.maxDimension,
    );
  }

  lastGeometryKeyRef.current = sceneData.geometryKey;
}
