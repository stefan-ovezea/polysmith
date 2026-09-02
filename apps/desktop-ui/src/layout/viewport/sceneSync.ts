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
  /** Set on Move-tool commit; cleared when the rebuild arrives (same
   *  keep-preview-alive semantics as pendingEndpointCommit). */
  pendingMoveCommit: MutableRef<boolean>;
  endpointDrag: MutableRef<EndpointDrag | null>;
  /** True while an endpoint-drag preview is mutating committed scene
   *  objects — mid-drag viewport_state syncs must not overwrite it. */
  dragPreviewMutating: MutableRef<boolean>;
  /** Same flag for the Move tool's live preview. */
  moveDragPreviewActive: MutableRef<boolean>;
  /** Set to force the next sync past the scene build-key guard (used to
   *  restore committed geometry after a cancelled live preview). */
  forceSceneRebuild: MutableRef<boolean>;
  activeSketchPlaneFrame: MutableRef<SketchPlaneFrame | null>;
  sketchEntityObjectById: MutableRef<Map<string, THREE.Line | THREE.LineLoop>>;
  sketchPointObjectById: MutableRef<Map<string, THREE.Mesh>>;
  sketchConstraintObjects: MutableRef<THREE.Object3D[]>;
  dragCursor: MutableRef<{ x: number; y: number } | null>;
  lastGeometryKey: MutableRef<string>;
  lastSceneBuildKey: MutableRef<string>;
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
  vertexObjects: MutableRef<THREE.Mesh[]>;
  cutPreviewObjects: MutableRef<THREE.Mesh[]>;
  toolpathLines: MutableRef<THREE.Object3D[]>;
  moveGizmoObjects: MutableRef<THREE.Object3D[]>;
  hiddenRelationPreviewDimensionIds: MutableRef<Set<string>>;
  selectedConstraint: MutableRef<SelectedConstraintState | null>;
  sketchEntityObjects: MutableRef<Array<THREE.Line | THREE.LineLoop>>;
  sketchDimensionObjects: MutableRef<THREE.Object3D[]>;
  dimensionObjectById: MutableRef<
    Map<string, { line: THREE.Group; label: THREE.Sprite }>
  >;
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
  activeCamSetupId?: string | null;
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

  const hadPendingCommit =
    params.refs.pendingEndpointCommit.current ||
    params.refs.pendingMoveCommit.current;
  params.refs.pendingEndpointCommit.current = false;
  params.refs.pendingMoveCommit.current = false;

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

  // A local preview is on-screen: keep the live positions, don't
  // overwrite them with committed-state data (would snap back a frame).
  if (refs.dragPreviewMutating.current || refs.moveDragPreviewActive.current) {
    refs.lastGeometryKey.current = sceneData.geometryKey;
    return true;
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
  const sceneBuildKey = viewportSceneBuildKey(params);
  if (
    params.sceneData &&
    params.refs.lastSceneBuildKey.current === sceneBuildKey &&
    !params.refs.forceSceneRebuild.current
  ) {
    return;
  }
  params.refs.forceSceneRebuild.current = false;

  resetViewportSceneGroups(params, groups);

  if (!params.sceneData) {
    params.refs.lastGeometryKey.current = "";
    params.refs.lastSceneBuildKey.current = "";
    return;
  }

  addModelSceneObjects(params, groups);
  addMoveGizmoSceneObject(params, groups.contentGroup);
  addSketchSceneObjectSet(params, groups.sketchGroup);
  params.refs.lastSceneBuildKey.current = sceneBuildKey;
}

function viewportSceneBuildKey({
  sceneData,
  displayedSketchDimensions,
  displayUnits,
  activeSketchPlaneId,
  showReferencePlanes,
  showStock,
  wcsOrientation,
  activeCamSetupId,
  moveGizmo,
  document,
}: SyncViewportSceneParams) {
  if (!sceneData) {
    return "";
  }
  // The CAM origin marker + stock box are drawn from the DOCUMENT (not
  // the viewport state), so the build key must include their values —
  // otherwise changing the origin in the setup panel never triggers a
  // scene rebuild and the marker stays put until some other update.
  // The RESOLVED WCS position matters too: face-anchored WCS picks and
  // laser pointer-offset changes move the marker without touching the
  // stock origin.
  const camSetup = document?.cam?.setups?.[0];
  const wcsPosition = document?.cam?.setups
    .find((setup) => setup.setup_id === activeCamSetupId)
    ?.wcs_origin?.position ?? camSetup?.wcs_origin?.position;
  const camSignature = camSetup
    ? [
        camSetup.stock?.origin?.join(",") ?? "",
        camSetup.stock?.size?.join(",") ?? "",
        camSetup.stock?.diameter ?? "",
        camSetup.stock?.length ?? "",
        camSetup.stock?.margin ?? 0,
        camSetup.stock?.type ?? "",
        wcsPosition?.join(",") ?? "",
        activeCamSetupId ?? "",
      ].join("|")
    : "nosetup";
  return [
    sceneData.geometryKey,
    displayUnits,
    activeSketchPlaneId ?? "",
    showReferencePlanes ? "refs:on" : "refs:off",
    showStock ? "stock:on" : "stock:off",
    "cam:" + camSignature,
    wcsOrientation,
    moveGizmoKey(moveGizmo),
    displayedSketchDimensions.map(sketchDimensionBuildKey).join("|"),
  ].join("::");
}

function moveGizmoKey(moveGizmo: MoveGizmoDescriptor | null | undefined) {
  if (!moveGizmo || moveGizmo.disabled) {
    return "move:none";
  }
  return JSON.stringify(moveGizmo);
}

function sketchDimensionBuildKey(dimension: SketchDimensionScene) {
  return [
    dimension.dimensionId,
    dimension.kind,
    dimension.entityId,
    dimension.label,
    dimension.driven ? "driven" : "",
    dimension.displayAs ?? "",
    dimension.anchorStart.join(":"),
    dimension.anchorEnd.join(":"),
    dimension.dimensionStart.join(":"),
    dimension.dimensionEnd.join(":"),
    dimension.labelPosition.join(":"),
  ].join(":");
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
    activeCamSetupId,
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
    activeCamSetupId,
  });

  addSolidSceneObjects({
    faces: sceneData.solidFaces,
    edges: sceneData.edges,
    vertices: sceneData.vertices,
    cutPreviews: sceneData.cutPreviews,
    contentGroup,
    faceMeshes: refs.faceMeshes.current,
    solidFaceVisuals: refs.solidFaceVisuals.current,
    solidFaceStates: refs.solidFaceStates.current,
    edgeLineObjects: refs.edgeLineObjects.current,
    vertexObjects: refs.vertexObjects.current,
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
    dimensionObjectById: refs.dimensionObjectById.current,
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
  refs.dragPreviewMutating.current = false;
  refs.moveDragPreviewActive.current = false;
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
