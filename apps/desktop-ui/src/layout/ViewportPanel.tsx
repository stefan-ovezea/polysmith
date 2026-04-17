import { useEffect, useMemo, useRef, useState } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { createViewportScene } from "@/lib";
import type {
  ConstraintType,
  DocumentState,
  SketchTool,
  ViewportState,
  ReferenceAxisScene,
  ReferencePlaneScene,
  ScenePrimitive,
  SolidFaceScene,
  SketchCircleScene,
  SketchConstraintScene,
  SketchDimensionScene,
  SketchLineScene,
  SketchPointScene,
  SketchProfileScene,
  SolidFacePlaneFrame,
  PrimitiveVisual,
  PrimitiveInteractionState,
  ReferencePlaneVisual,
  ReferencePlaneInteractionState,
  SketchPlaneFrame,
  ViewportContextMenuState,
  SketchPreviewPoint,
} from "@/types";
import {
  applyPrimitiveVisualState,
  applyReferencePlaneVisualState,
  buildPrimitiveObject,
  buildReferenceAxisObject,
  buildReferencePlaneObject,
  buildSketchCircleObject,
  buildSketchConstraintObject,
  buildSketchDimensionObject,
  buildSketchLineObject,
  buildSketchPointObject,
  buildSketchProfileObject,
  buildSolidFaceObject,
  disposeGroup,
  disposeMaterial,
  distanceBetweenPoints,
  frameCamera,
  frameCameraToSketchPlane,
  projectWorldPointToViewport,
  resolveSketchPlanePoint,
  SKETCH_SNAP_DISTANCE,
  themeColor,
  toWorldPoint,
} from "@/utils";

interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
  onStartSketch: (referenceId: string) => Promise<void>;
  onStartSketchOnFace: (
    faceId: string,
    planeFrame: SolidFacePlaneFrame,
  ) => Promise<void>;
  onAddSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
  ) => Promise<void>;
  onAddSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
  ) => Promise<void>;
  onAddSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
  ) => Promise<void>;
  onSelectSketchEntity: (entityId: string) => Promise<void>;
  onPickSketchPoint: (
    pointId: string,
    entityId: string,
    kind: "endpoint" | "center",
  ) => Promise<void>;
  armedSketchConstraint:
    | null
    | { kind: "horizontal" | "vertical" | "clear" }
    | {
        kind: "equal_length" | "perpendicular" | "parallel";
        firstLineId: string | null;
      }
    | { kind: "coincident"; firstPointId: string | null };
  onCancelSketchConstraint: () => void;
  onClearSketchConstraint: (
    kind: ConstraintType,
    entityId: string,
    relatedEntityId: string | null,
  ) => Promise<void>;
  onSelectSketchDimension: (dimensionId: string) => Promise<void>;
  onUpdateSketchDimension: (
    dimensionId: string,
    value: number,
  ) => Promise<void>;
  onSelectSketchProfile: (profileId: string) => Promise<void>;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onFinishSketch: () => Promise<void>;
}

export function ViewportPanel({
  status,
  document,
  viewport,
  onSelectPrimitive,
  onSelectReference,
  onSelectFace,
  onStartSketch,
  onStartSketchOnFace,
  onAddSketchLine,
  onAddSketchRectangle,
  onAddSketchCircle,
  onSelectSketchEntity,
  onPickSketchPoint,
  armedSketchConstraint,
  onCancelSketchConstraint,
  onClearSketchConstraint,
  onSelectSketchDimension,
  onUpdateSketchDimension,
  onSelectSketchProfile,
  onSetSketchTool,
  onFinishSketch,
}: ViewportPanelProps) {
  const [showReferencePlanes, setShowReferencePlanes] = useState(true);
  const [contextMenu, setContextMenu] =
    useState<ViewportContextMenuState | null>(null);
  const [sketchSnapLabel, setSketchSnapLabel] = useState<string | null>(null);
  const [dimensionDraftValue, setDimensionDraftValue] = useState("");
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dimensionEditorRef = useRef<HTMLFormElement | null>(null);
  const dimensionInputRef = useRef<HTMLInputElement | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.PerspectiveCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const contentGroupRef = useRef<THREE.Group | null>(null);
  const referenceGroupRef = useRef<THREE.Group | null>(null);
  const sketchGroupRef = useRef<THREE.Group | null>(null);
  const previewLineRef = useRef<THREE.Line | null>(null);
  const previewCircleRef = useRef<THREE.LineLoop | null>(null);
  const lineDraftStartRef = useRef<[number, number] | null>(null);
  const previousReferencePlaneVisibilityRef = useRef<boolean | null>(null);
  const primitiveVisualsRef = useRef(new Map<string, PrimitiveVisual>());
  const primitiveStatesRef = useRef(
    new Map<string, PrimitiveInteractionState>(),
  );
  const referencePlaneVisualsRef = useRef(
    new Map<string, ReferencePlaneVisual>(),
  );
  const referencePlaneStatesRef = useRef(
    new Map<string, ReferencePlaneInteractionState>(),
  );
  const gridRef = useRef<THREE.GridHelper | null>(null);
  const meshesRef = useRef<THREE.Mesh[]>([]);
  const referencePlaneMeshesRef = useRef<THREE.Mesh[]>([]);
  const sketchEntityObjectsRef = useRef<Array<THREE.Line | THREE.LineLoop>>([]);
  const sketchDimensionObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchConstraintObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchPointObjectsRef = useRef<THREE.Mesh[]>([]);
  const sketchProfileMeshesRef = useRef<THREE.Mesh[]>([]);
  const faceMeshesRef = useRef<THREE.Mesh[]>([]);
  const lastGeometryKeyRef = useRef("");
  const selectPrimitiveRef = useRef(onSelectPrimitive);
  const selectReferenceRef = useRef(onSelectReference);
  const selectFaceRef = useRef(onSelectFace);
  const startSketchRef = useRef(onStartSketch);
  const startSketchOnFaceRef = useRef(onStartSketchOnFace);
  const addSketchLineRef = useRef(onAddSketchLine);
  const addSketchRectangleRef = useRef(onAddSketchRectangle);
  const addSketchCircleRef = useRef(onAddSketchCircle);
  const selectSketchEntityRef = useRef(onSelectSketchEntity);
  const pickSketchPointRef = useRef(onPickSketchPoint);
  const selectSketchDimensionRef = useRef(onSelectSketchDimension);
  const updateSketchDimensionRef = useRef(onUpdateSketchDimension);
  const selectSketchProfileRef = useRef(onSelectSketchProfile);
  const selectedSketchDimensionRef = useRef<SketchDimensionScene | null>(null);
  const setSketchToolRef = useRef(onSetSketchTool);
  const finishSketchRef = useRef(onFinishSketch);
  const armedSketchConstraintRef = useRef(armedSketchConstraint);
  const cancelSketchConstraintRef = useRef(onCancelSketchConstraint);
  const clearSketchConstraintRef = useRef(onClearSketchConstraint);
  const activeSketchToolRef = useRef<SketchTool>("select");
  const sketchSnapCandidatesRef = useRef<
    Array<{ local: [number, number]; label: string }>
  >([]);
  const sceneData = useMemo(
    () =>
      viewport?.has_active_document ? createViewportScene(viewport) : null,
    [viewport],
  );
  const hasActiveDocument = Boolean(viewport?.has_active_document);
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? "select";
  const sketchFeature = useMemo(
    () =>
      document?.feature_history.find(
        (feature) => feature.feature_id === document.active_sketch_feature_id,
      ) ?? null,
    [document],
  );
  const selectedPrimitiveLabel = useMemo(() => {
    const selectedBox = viewport?.boxes.find((box) => box.is_selected);
    if (selectedBox) {
      return selectedBox.label;
    }

    const selectedCylinder = viewport?.cylinders.find(
      (cylinder) => cylinder.is_selected,
    );
    if (selectedCylinder) {
      return selectedCylinder.label;
    }

    const selectedPolygonExtrude = viewport?.polygon_extrudes.find(
      (primitive) => primitive.is_selected,
    );
    return selectedPolygonExtrude?.label ?? null;
  }, [viewport]);
  const selectedReference = useMemo(
    () =>
      viewport?.reference_planes.find(
        (referencePlane) => referencePlane.is_selected,
      ) ?? null,
    [viewport],
  );
  const selectedSketchProfile = useMemo(
    () =>
      viewport?.sketch_profiles.find((profile) => profile.is_selected) ?? null,
    [viewport],
  );
  const selectedSketchDimension = useMemo(
    () =>
      document?.selected_sketch_dimension_id
        ? (sceneData?.sketchDimensions.find(
            (dimension) =>
              dimension.dimensionId === document.selected_sketch_dimension_id,
          ) ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sceneData],
  );
  const selectedSketchDimensionValue = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.value ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const sketchSnapCandidates = useMemo(() => {
    if (!sketchFeature?.sketch_parameters) {
      return [];
    }

    const candidates: Array<{ local: [number, number]; label: string }> = [
      { local: [0, 0], label: "Origin" },
    ];
    for (const line of sketchFeature.sketch_parameters.lines) {
      candidates.push({
        local: [line.start_x, line.start_y],
        label:
          line.constraint === "horizontal" || line.constraint === "vertical"
            ? `${line.line_id} (${line.constraint})`
            : line.line_id,
      });
      candidates.push({
        local: [line.end_x, line.end_y],
        label: line.line_id,
      });
    }
    for (const circle of sketchFeature.sketch_parameters.circles) {
      candidates.push({
        local: [circle.center_x, circle.center_y],
        label: circle.circle_id,
      });
    }
    return candidates;
  }, [sketchFeature]);
  const activeSketchPlaneFrame =
    sketchFeature?.sketch_parameters?.plane_frame ?? null;

  function clearPreviewLine() {
    const previewLine = previewLineRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewLine || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewLine);
    previewLine.geometry.dispose();
    disposeMaterial(previewLine.material);
    previewLineRef.current = null;
  }

  function clearPreviewCircle() {
    const previewCircle = previewCircleRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewCircle || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewCircle);
    previewCircle.geometry.dispose();
    disposeMaterial(previewCircle.material);
    previewCircleRef.current = null;
  }

  function resolveSnappedSketchPoint(rawPoint: {
    local: [number, number];
    world: [number, number, number];
  }) {
    let closestCandidate: { local: [number, number]; label: string } | null =
      null;
    let closestDistance = Number.POSITIVE_INFINITY;

    for (const candidate of sketchSnapCandidatesRef.current) {
      const distance = distanceBetweenPoints(rawPoint.local, candidate.local);
      if (distance < closestDistance) {
        closestDistance = distance;
        closestCandidate = candidate;
      }
    }

    if (closestCandidate && closestDistance <= SKETCH_SNAP_DISTANCE) {
      return {
        local: closestCandidate.local,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          closestCandidate.local,
          activeSketchPlaneFrame,
        ),
        snapLabel: closestCandidate.label,
      } satisfies SketchPreviewPoint;
    }

    return {
      local: rawPoint.local,
      world: rawPoint.world,
      snapLabel: null,
    } satisfies SketchPreviewPoint;
  }

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

  useEffect(() => {
    selectPrimitiveRef.current = onSelectPrimitive;
    selectReferenceRef.current = onSelectReference;
    selectFaceRef.current = onSelectFace;
    startSketchRef.current = onStartSketch;
    startSketchOnFaceRef.current = onStartSketchOnFace;
    addSketchLineRef.current = onAddSketchLine;
    addSketchRectangleRef.current = onAddSketchRectangle;
    addSketchCircleRef.current = onAddSketchCircle;
    selectSketchEntityRef.current = onSelectSketchEntity;
    pickSketchPointRef.current = onPickSketchPoint;
    selectSketchDimensionRef.current = onSelectSketchDimension;
    updateSketchDimensionRef.current = onUpdateSketchDimension;
    selectSketchProfileRef.current = onSelectSketchProfile;
    setSketchToolRef.current = onSetSketchTool;
    finishSketchRef.current = onFinishSketch;
    armedSketchConstraintRef.current = armedSketchConstraint;
    cancelSketchConstraintRef.current = onCancelSketchConstraint;
    clearSketchConstraintRef.current = onClearSketchConstraint;
  }, [
    onSelectPrimitive,
    onSelectReference,
    onSelectFace,
    onStartSketch,
    onStartSketchOnFace,
    onAddSketchLine,
    onAddSketchRectangle,
    onAddSketchCircle,
    onSelectSketchEntity,
    onPickSketchPoint,
    onSelectSketchDimension,
    onUpdateSketchDimension,
    onSelectSketchProfile,
    onSetSketchTool,
    onFinishSketch,
    armedSketchConstraint,
    onCancelSketchConstraint,
    onClearSketchConstraint,
  ]);

  useEffect(() => {
    activeSketchToolRef.current = activeSketchTool;
    sketchSnapCandidatesRef.current = sketchSnapCandidates;
  }, [activeSketchTool, sketchSnapCandidates]);

  useEffect(() => {
    selectedSketchDimensionRef.current = selectedSketchDimension;
  }, [selectedSketchDimension]);

  useEffect(() => {
    if (selectedSketchDimensionValue === null) {
      setDimensionDraftValue("");
      return;
    }

    setDimensionDraftValue(String(selectedSketchDimensionValue));
  }, [selectedSketchDimensionValue, document?.selected_sketch_dimension_id]);

  useEffect(() => {
    if (!selectedSketchDimension) {
      return;
    }

    const input = dimensionInputRef.current;
    if (!input) {
      return;
    }

    input.focus();
    input.select();
  }, [selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    const host = hostRef.current;
    const canvas = canvasRef.current;

    if (!host || !canvas) {
      return undefined;
    }

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
    });
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(46, 1, 0.1, 10000);
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId = 0;

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    contentGroupRef.current = contentGroup;
    referenceGroupRef.current = referenceGroup;
    sketchGroupRef.current = sketchGroup;

    renderer.setPixelRatio(window.devicePixelRatio);
    scene.add(contentGroup);
    scene.add(referenceGroup);
    scene.add(sketchGroup);
    scene.add(
      new THREE.AmbientLight(
        themeColor("--color-primary-soft", "#8defff"),
        1.15,
      ),
    );

    const keyLight = new THREE.DirectionalLight(
      themeColor("--color-primary-edge-active", "#d8fbff"),
      1.35,
    );
    keyLight.position.set(1.2, 1.8, 1.4);
    scene.add(keyLight);

    const rimLight = new THREE.DirectionalLight(
      themeColor("--color-primary-fixed-dim", "#00d8f1"),
      0.8,
    );
    rimLight.position.set(-1.5, 0.8, -1.1);
    scene.add(rimLight);

    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.screenSpacePanning = true;
    controls.minDistance = 24;
    controls.maxDistance = 6000;
    controls.mouseButtons.LEFT = null;
    controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
    controls.mouseButtons.RIGHT = null;
    controls.addEventListener("start", () => {
      canvas.classList.add("cad-viewport-canvas-dragging");
    });
    controls.addEventListener("end", () => {
      canvas.classList.remove("cad-viewport-canvas-dragging");
    });

    function resizeRenderer() {
      const width = Math.max(host?.clientWidth ?? 0, 1);
      const height = Math.max(host?.clientHeight ?? 0, 1);
      renderer.setSize(width, height, false);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
    }

    function render() {
      controls.update();
      renderer.render(scene, camera);

      const editor = dimensionEditorRef.current;
      const dimension = selectedSketchDimensionRef.current;
      if (!editor || !dimension) {
        if (editor) {
          editor.style.opacity = "0";
        }
        return;
      }

      const projectedPosition = projectWorldPointToViewport(
        dimension.labelPosition,
        camera,
        renderer,
      );

      if (!projectedPosition) {
        editor.style.opacity = "0";
        return;
      }

      editor.style.opacity = "1";
      editor.style.transform = `translate(${projectedPosition.x}px, ${projectedPosition.y}px) translate(-50%, -50%)`;
    }

    function intersectSceneTargets(event: PointerEvent) {
      const rect = renderer.domElement.getBoundingClientRect();
      pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(pointer, camera);
      raycaster.params.Line = { threshold: 1.75 };

      if (activeSketchPlaneId) {
        const [sketchDimensionHit] = raycaster.intersectObjects(
          sketchDimensionObjectsRef.current,
          false,
        );
        const sketchDimensionId =
          sketchDimensionHit?.object.userData.sketchDimensionId;
        if (typeof sketchDimensionId === "string") {
          return { kind: "sketch_dimension" as const, id: sketchDimensionId };
        }

        const [sketchConstraintHit] = raycaster.intersectObjects(
          sketchConstraintObjectsRef.current,
          false,
        );
        const sketchConstraintId =
          sketchConstraintHit?.object.userData.sketchConstraintId;
        if (typeof sketchConstraintId === "string") {
          return {
            kind: "sketch_constraint" as const,
            id: sketchConstraintId,
            constraintKind:
              sketchConstraintHit.object.userData.sketchConstraintKind,
            entityId:
              sketchConstraintHit.object.userData.sketchConstraintEntityId,
            relatedEntityId:
              sketchConstraintHit.object.userData
                .sketchConstraintRelatedEntityId ?? null,
          };
        }

        if (armedSketchConstraintRef.current?.kind === "coincident") {
          const [sketchPointHit] = raycaster.intersectObjects(
            sketchPointObjectsRef.current,
            false,
          );
          const sketchPointId = sketchPointHit?.object.userData.sketchPointId;
          if (typeof sketchPointId === "string") {
            return {
              kind: "sketch_point" as const,
              id: sketchPointId,
              entityId: sketchPointHit.object.userData.sketchPointEntityId,
              pointKind: sketchPointHit.object.userData.sketchPointKind,
            };
          }
        }

        const [sketchEntityHit] = raycaster.intersectObjects(
          sketchEntityObjectsRef.current,
          false,
        );
        const sketchEntityId = sketchEntityHit?.object.userData.sketchEntityId;
        const sketchEntityKind =
          sketchEntityHit?.object.userData.sketchEntityKind;
        if (typeof sketchEntityId === "string") {
          return {
            kind: "sketch_entity" as const,
            id: sketchEntityId,
            entityKind:
              typeof sketchEntityKind === "string" ? sketchEntityKind : null,
          };
        }

        const [profileHit] = raycaster.intersectObjects(
          sketchProfileMeshesRef.current,
          false,
        );
        const profileId = profileHit?.object.userData.sketchProfileId;
        if (typeof profileId === "string") {
          return { kind: "sketch_profile" as const, id: profileId };
        }
      }

      const [referenceHit] = raycaster.intersectObjects(
        referencePlaneMeshesRef.current,
        false,
      );
      const referenceId = referenceHit?.object.userData.referenceId;
      if (typeof referenceId === "string") {
        return { kind: "reference" as const, id: referenceId };
      }

      const [faceHit] = raycaster.intersectObjects(
        faceMeshesRef.current,
        false,
      );
      const faceId = faceHit?.object.userData.faceId;
      if (typeof faceId === "string") {
        return { kind: "face" as const, id: faceId };
      }

      const [primitiveHit] = raycaster.intersectObjects(
        meshesRef.current,
        false,
      );
      const primitiveId = primitiveHit?.object.userData.primitiveId;
      return typeof primitiveId === "string"
        ? { kind: "primitive" as const, id: primitiveId }
        : null;
    }

    function handlePointerDown(event: PointerEvent) {
      setContextMenu(null);

      if (event.button === 1) {
        controls.mouseButtons.MIDDLE =
          event.ctrlKey || event.metaKey ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      pointerDown = { x: event.clientX, y: event.clientY };
    }

    function handlePointerMove(event: PointerEvent) {
      if (activeSketchPlaneId) {
        if (activeSketchToolRef.current === "select") {
          clearPreviewLine();
          clearPreviewCircle();
          setSketchSnapLabel(null);
          return;
        }

        const draftStart = lineDraftStartRef.current;
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }

        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);

        if (!draftStart) {
          const hit = intersectSceneTargets(event);
          setHoveredPrimitive(null);
          setHoveredReference(null);
          return;
        }

        const sketchGroupRefValue = sketchGroupRef.current;
        if (!sketchGroupRefValue) {
          return;
        }

        clearPreviewLine();
        clearPreviewCircle();
        if (activeSketchToolRef.current === "circle") {
          const radius = distanceBetweenPoints(draftStart, sketchPoint.local);
          if (radius > 0.001) {
            const preview = buildSketchCircleObject({
              circleId: "preview-circle",
              planeId: activeSketchPlaneId,
              center: toWorldPoint(
                activeSketchPlaneId,
                draftStart,
                activeSketchPlaneFrame,
              ),
              radius,
              isSelected: false,
            });
            previewCircleRef.current = preview;
            sketchGroupRefValue.add(preview);
          }
        } else {
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints([
              new THREE.Vector3(
                ...toWorldPoint(
                  activeSketchPlaneId,
                  draftStart,
                  activeSketchPlaneFrame,
                ),
              ),
              new THREE.Vector3(...sketchPoint.world),
            ]),
            new THREE.LineBasicMaterial({
              color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
              transparent: true,
              opacity: 0.88,
            }),
          );
          previewLineRef.current = preview;
          sketchGroupRefValue.add(preview);
        }
        return;
      }

      const hit = intersectSceneTargets(event);
      if (hit?.kind === "sketch_dimension" || hit?.kind === "sketch_entity") {
        setHoveredReference(null);
        setHoveredPrimitive(null);
        return;
      }
      setHoveredReference(hit?.kind === "reference" ? hit.id : null);
      setHoveredPrimitive(hit?.kind === "primitive" ? hit.id : null);
    }

    function handlePointerLeave() {
      pointerDown = null;
      setSketchSnapLabel(null);
      if (!activeSketchPlaneId) {
        setHoveredReference(null);
        setHoveredPrimitive(null);
      }
    }

    function handlePointerUp(event: PointerEvent) {
      if (event.button === 1) {
        controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
        pointerDown = null;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      if (!pointerDown) {
        return;
      }

      const deltaX = Math.abs(event.clientX - pointerDown.x);
      const deltaY = Math.abs(event.clientY - pointerDown.y);
      pointerDown = null;

      if (deltaX > 4 || deltaY > 4) {
        return;
      }

      if (activeSketchPlaneId) {
        const hit = intersectSceneTargets(event);
        if (activeSketchToolRef.current === "select") {
          if (
            armedSketchConstraintRef.current &&
            hit?.kind === "sketch_entity" &&
            hit.entityKind === "line"
          ) {
            void selectSketchEntityRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_point") {
            void pickSketchPointRef.current(
              hit.id,
              hit.entityId,
              hit.pointKind,
            );
            return;
          }

          if (hit?.kind === "sketch_dimension") {
            void selectSketchDimensionRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_constraint") {
            void clearSketchConstraintRef.current(
              hit.constraintKind,
              hit.entityId,
              hit.relatedEntityId,
            );
            return;
          }

          if (hit?.kind === "sketch_profile") {
            void selectSketchProfileRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_entity") {
            void selectSketchEntityRef.current(hit.id);
          }
          return;
        }

        if (!lineDraftStartRef.current && hit?.kind === "sketch_dimension") {
          void selectSketchDimensionRef.current(hit.id);
          return;
        }

        if (!lineDraftStartRef.current && hit?.kind === "sketch_entity") {
          void selectSketchEntityRef.current(hit.id);
          return;
        }

        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }
        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);

        if (!lineDraftStartRef.current) {
          lineDraftStartRef.current = sketchPoint.local;
          return;
        }

        const [startX, startY] = lineDraftStartRef.current;
        clearPreviewLine();
        clearPreviewCircle();
        if (activeSketchToolRef.current === "rectangle") {
          lineDraftStartRef.current = null;
          void addSketchRectangleRef.current(
            startX,
            startY,
            sketchPoint.local[0],
            sketchPoint.local[1],
          );
          return;
        }

        if (activeSketchToolRef.current === "circle") {
          lineDraftStartRef.current = null;
          const radius = distanceBetweenPoints(
            [startX, startY],
            sketchPoint.local,
          );
          void addSketchCircleRef.current(startX, startY, radius);
          return;
        }

        lineDraftStartRef.current = sketchPoint.local;
        void addSketchLineRef.current(
          startX,
          startY,
          sketchPoint.local[0],
          sketchPoint.local[1],
        );
        return;
      }

      const hit = intersectSceneTargets(event);
      if (hit?.kind === "reference") {
        void selectReferenceRef.current(hit.id);
        return;
      }

      if (hit?.kind === "face") {
        void selectFaceRef.current(hit.id);
        return;
      }

      if (hit?.kind === "primitive") {
        void selectPrimitiveRef.current(hit.id);
      }
    }

    function handleContextMenu(event: MouseEvent) {
      event.preventDefault();

      if (activeSketchPlaneId) {
        setContextMenu(null);
        return;
      }

      const hit = intersectSceneTargets(event as PointerEvent);
      if (hit?.kind !== "reference" && hit?.kind !== "face") {
        setContextMenu(null);
        return;
      }

      const rect = renderer.domElement.getBoundingClientRect();
      setContextMenu({
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
        referenceId: hit.kind === "reference" ? hit.id : null,
        faceId: hit.kind === "face" ? hit.id : null,
      });
    }

    const resizeObserver = new ResizeObserver(() => {
      resizeRenderer();
      render();
    });

    resizeObserver.observe(host);
    renderer.domElement.addEventListener("pointerdown", handlePointerDown);
    renderer.domElement.addEventListener("pointermove", handlePointerMove);
    renderer.domElement.addEventListener("pointerleave", handlePointerLeave);
    renderer.domElement.addEventListener("pointerup", handlePointerUp);
    renderer.domElement.addEventListener("contextmenu", handleContextMenu);
    resizeRenderer();

    const animate = () => {
      render();
      frameId = window.requestAnimationFrame(animate);
    };

    frameId = window.requestAnimationFrame(animate);

    return () => {
      window.cancelAnimationFrame(frameId);
      resizeObserver.disconnect();
      renderer.domElement.removeEventListener("pointerdown", handlePointerDown);
      renderer.domElement.removeEventListener("pointermove", handlePointerMove);
      renderer.domElement.removeEventListener(
        "pointerleave",
        handlePointerLeave,
      );
      renderer.domElement.removeEventListener("pointerup", handlePointerUp);
      renderer.domElement.removeEventListener("contextmenu", handleContextMenu);
      controls.dispose();
      disposeGroup(contentGroup);
      disposeGroup(referenceGroup);
      disposeGroup(sketchGroup);
      renderer.dispose();
      gridRef.current?.geometry.dispose();
      disposeMaterial(gridRef.current?.material ?? []);
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      contentGroupRef.current = null;
      referenceGroupRef.current = null;
      sketchGroupRef.current = null;
      primitiveVisualsRef.current.clear();
      primitiveStatesRef.current.clear();
      referencePlaneVisualsRef.current.clear();
      referencePlaneStatesRef.current.clear();
      referencePlaneMeshesRef.current = [];
      sketchEntityObjectsRef.current = [];
      sketchDimensionObjectsRef.current = [];
      sketchConstraintObjectsRef.current = [];
      sketchPointObjectsRef.current = [];
      sketchProfileMeshesRef.current = [];
      meshesRef.current = [];
      faceMeshesRef.current = [];
      gridRef.current = null;
      previewLineRef.current = null;
      previewCircleRef.current = null;
      lineDraftStartRef.current = null;
      lastGeometryKeyRef.current = "";
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    const scene = sceneRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    const contentGroup = contentGroupRef.current;
    const referenceGroup = referenceGroupRef.current;
    const sketchGroup = sketchGroupRef.current;

    if (
      !scene ||
      !camera ||
      !controls ||
      !contentGroup ||
      !referenceGroup ||
      !sketchGroup
    ) {
      return;
    }

    disposeGroup(contentGroup);
    disposeGroup(referenceGroup);
    disposeGroup(sketchGroup);
    primitiveVisualsRef.current.clear();
    primitiveStatesRef.current.clear();
    referencePlaneVisualsRef.current.clear();
    referencePlaneStatesRef.current.clear();
    referencePlaneMeshesRef.current = [];
    sketchEntityObjectsRef.current = [];
    sketchDimensionObjectsRef.current = [];
    sketchConstraintObjectsRef.current = [];
    sketchPointObjectsRef.current = [];
    sketchProfileMeshesRef.current = [];
    meshesRef.current = [];
    faceMeshesRef.current = [];
    previewLineRef.current = null;
    previewCircleRef.current = null;

    if (gridRef.current) {
      scene.remove(gridRef.current);
      gridRef.current.geometry.dispose();
      disposeMaterial(gridRef.current.material);
    }

    gridRef.current = null;

    if (!sceneData) {
      lastGeometryKeyRef.current = "";
      return;
    }

    const nextGrid = new THREE.GridHelper(
      Math.max(sceneData.bounds.maxDimension * 2, 200),
      20,
      themeColor("--color-primary-fixed-dim", "#00d8f1"),
      themeColor("--color-primary-emissive-hover", "#214147"),
    );
    nextGrid.position.set(
      sceneData.bounds.center[0],
      0,
      sceneData.bounds.center[2],
    );
    scene.add(nextGrid);
    gridRef.current = nextGrid;

    for (const primitive of sceneData.primitives) {
      const object = buildPrimitiveObject(primitive);
      meshesRef.current.push(object.mesh);
      primitiveVisualsRef.current.set(primitive.primitiveId, object.visual);
      primitiveStatesRef.current.set(primitive.primitiveId, {
        isSelected: primitive.isSelected,
        isHovered: false,
      });
      contentGroup.add(object.mesh);
      contentGroup.add(object.edges);
    }

    for (const reference of sceneData.references) {
      if (reference.kind === "reference_plane") {
        if (!showReferencePlanes) {
          continue;
        }

        const object = buildReferencePlaneObject(reference);
        referencePlaneMeshesRef.current.push(object.mesh);
        referencePlaneVisualsRef.current.set(
          reference.referenceId,
          object.visual,
        );
        referencePlaneStatesRef.current.set(reference.referenceId, {
          isSelected: reference.isSelected,
          isHovered: false,
          isActiveSketchPlane: reference.isActiveSketchPlane,
        });
        referenceGroup.add(object.mesh);
        referenceGroup.add(object.edges);
        continue;
      }

      const axisObject = buildReferenceAxisObject(reference);
      referenceGroup.add(axisObject.line);
    }

    for (const face of sceneData.solidFaces) {
      const faceObject = buildSolidFaceObject(face);
      faceMeshesRef.current.push(faceObject);
      contentGroup.add(faceObject);
    }

    for (const sketchLine of sceneData.sketchLines) {
      const sketchLineObject = buildSketchLineObject(sketchLine);
      sketchEntityObjectsRef.current.push(sketchLineObject);
      sketchGroup.add(sketchLineObject);
    }

    for (const sketchCircle of sceneData.sketchCircles) {
      const sketchCircleObject = buildSketchCircleObject(sketchCircle);
      sketchEntityObjectsRef.current.push(sketchCircleObject);
      sketchGroup.add(sketchCircleObject);
    }

    for (const sketchDimension of sceneData.sketchDimensions) {
      const sketchDimensionObject = buildSketchDimensionObject(sketchDimension);
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.line);
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.label);
      sketchGroup.add(sketchDimensionObject.line);
      sketchGroup.add(sketchDimensionObject.label);
    }

    for (const sketchConstraint of sceneData.sketchConstraints) {
      const sketchConstraintObject =
        buildSketchConstraintObject(sketchConstraint);
      sketchConstraintObjectsRef.current.push(sketchConstraintObject);
      sketchGroup.add(sketchConstraintObject);
    }

    for (const sketchProfile of sceneData.sketchProfiles) {
      const sketchProfileMesh = buildSketchProfileObject(sketchProfile);
      sketchProfileMeshesRef.current.push(sketchProfileMesh);
      sketchGroup.add(sketchProfileMesh);
    }

    for (const sketchPoint of sceneData.sketchPoints) {
      const sketchPointObject = buildSketchPointObject(sketchPoint);
      sketchPointObjectsRef.current.push(sketchPointObject);
      sketchGroup.add(sketchPointObject);
    }

    syncPrimitiveVisuals();
    syncReferencePlaneVisuals();

    if (sceneData.geometryKey !== lastGeometryKeyRef.current) {
      if (!activeSketchPlaneId) {
        frameCamera(
          camera,
          controls,
          sceneData.bounds.center,
          sceneData.bounds.maxDimension,
        );
      }

      lastGeometryKeyRef.current = sceneData.geometryKey;
    }
  }, [sceneData, showReferencePlanes]);

  useEffect(() => {
    lineDraftStartRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    setSketchSnapLabel(null);
  }, [activeSketchPlaneId, activeSketchTool]);

  useEffect(() => {
    if (!activeSketchPlaneId) {
      return undefined;
    }

    function handleKeyDown(event: KeyboardEvent) {
      const target = event.target;
      if (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      ) {
        return;
      }

      if (event.code === "Escape") {
        event.preventDefault();
        if (armedSketchConstraintRef.current) {
          cancelSketchConstraintRef.current();
          return;
        }
        lineDraftStartRef.current = null;
        clearPreviewLine();
        clearPreviewCircle();
        setSketchSnapLabel(null);
        void setSketchToolRef.current("select");
        return;
      }

      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) {
        return;
      }

      if (event.code === "KeyL") {
        event.preventDefault();
        void setSketchToolRef.current("line");
        return;
      }

      if (event.code === "KeyR") {
        event.preventDefault();
        void setSketchToolRef.current("rectangle");
        return;
      }

      if (event.code === "KeyC") {
        event.preventDefault();
        void setSketchToolRef.current("circle");
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      if (previousReferencePlaneVisibilityRef.current === null) {
        previousReferencePlaneVisibilityRef.current = showReferencePlanes;
      }

      if (showReferencePlanes) {
        setShowReferencePlanes(false);
      }
      return;
    }

    if (previousReferencePlaneVisibilityRef.current !== null) {
      setShowReferencePlanes(previousReferencePlaneVisibilityRef.current);
      previousReferencePlaneVisibilityRef.current = null;
    }
  }, [activeSketchPlaneId, showReferencePlanes]);

  useEffect(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;

    if (!camera || !controls || !sceneData || !activeSketchPlaneId) {
      return;
    }

    frameCameraToSketchPlane(
      camera,
      controls,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sceneData.bounds.maxDimension,
    );
  }, [activeSketchPlaneId, activeSketchPlaneFrame, sceneData]);

  function handleFinishSketch() {
    lineDraftStartRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    void finishSketchRef.current();
  }

  async function handleCreateSketchFromContextMenu() {
    if (contextMenu?.referenceId) {
      setContextMenu(null);
      await selectReferenceRef.current(contextMenu.referenceId);
      await startSketchRef.current(contextMenu.referenceId);
      return;
    }

    if (!contextMenu?.faceId) {
      return;
    }

    setContextMenu(null);
    await selectFaceRef.current(contextMenu.faceId);

    const solidFace = sceneData?.solidFaces.find(
      (face) => face.faceId === contextMenu.faceId,
    );
    if (!solidFace) {
      return;
    }

    await startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
  }

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;

  async function handleSubmitDimensionEdit() {
    if (!selectedSketchDimension) {
      return;
    }

    const nextValue = Number(dimensionDraftValue);
    if (!Number.isFinite(nextValue) || nextValue <= 0) {
      return;
    }

    await updateSketchDimensionRef.current(
      selectedSketchDimension.dimensionId,
      nextValue,
    );
  }

  const selectedSketchDimensionTitle = selectedSketchDimension
    ? selectedSketchDimension.kind === "line_length"
      ? "Length"
      : "Radius"
    : null;

  return (
    <section className="relative flex h-full min-h-0 flex-col overflow-hidden">
      <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      <div className="pointer-events-none absolute left-4 top-4 z-10">
        <button
          type="button"
          className={
            showReferencePlanes
              ? "pointer-events-auto cad-tool-button cad-tool-button-active"
              : "pointer-events-auto cad-tool-button"
          }
          onClick={() => {
            setShowReferencePlanes((current) => !current);
          }}
        >
          {showReferencePlanes ? "Hide Origin Planes" : "Show Origin Planes"}
        </button>
      </div>
      <div
        ref={hostRef}
        className="absolute inset-0 min-h-0 min-w-0 overflow-hidden rounded-[18px]"
      >
        {contextMenu ? (
          <div
            className="cad-context-menu absolute z-20 min-w-[160px] rounded-2xl p-1.5 backdrop-blur-xl"
            style={{
              left: contextMenu.x,
              top: contextMenu.y,
              transform: "translate(8px, 8px)",
            }}
          >
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={handleCreateSketchFromContextMenu}
            >
              Create Sketch
            </button>
          </div>
        ) : null}
        <canvas
          ref={canvasRef}
          className={`cad-viewport-canvas absolute inset-0 h-full w-full ${
            activeSketchPlaneId && activeSketchTool !== "select"
              ? "cad-viewport-canvas-drawing"
              : ""
          }`}
        />
        {selectedSketchDimension && activeSketchPlaneId ? (
          <form
            ref={dimensionEditorRef}
            className="pointer-events-auto absolute z-20 flex min-w-[188px] items-center gap-2 rounded-2xl border border-white/15 bg-black/75 px-3 py-2 backdrop-blur-xl"
            style={{
              left: 0,
              top: 0,
              opacity: 0,
            }}
            onSubmit={(event) => {
              event.preventDefault();
              void handleSubmitDimensionEdit();
            }}
          >
            <span className="shrink-0 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
              {selectedSketchDimensionTitle}
            </span>
            <input
              ref={dimensionInputRef}
              className="cad-input h-9 min-w-0 flex-1"
              type="number"
              min="0.01"
              step="0.01"
              value={dimensionDraftValue}
              onChange={(event) => {
                setDimensionDraftValue(event.target.value);
              }}
              onFocus={(event) => {
                event.currentTarget.select();
              }}
              onKeyDown={(event) => {
                if (event.key !== "Escape") {
                  return;
                }

                event.preventDefault();
                setDimensionDraftValue(
                  selectedSketchDimensionValue !== null
                    ? String(selectedSketchDimensionValue)
                    : "",
                );
                event.currentTarget.blur();
              }}
            />
            <span className="shrink-0 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
              mm
            </span>
            <button
              type="submit"
              className="cad-action-primary shrink-0"
              disabled={Number(dimensionDraftValue) <= 0}
            >
              Set
            </button>
          </form>
        ) : null}
        {!hasActiveDocument ? (
          <div
            className="absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-strong)" }}
          >
            <div className="text-center">
              <p className="cad-kicker">Viewport</p>
              <p className="mt-4 text-sm text-on-surface-muted">
                No active document to render.
              </p>
            </div>
          </div>
        ) : null}
        {status === "starting" ? (
          <div
            className="pointer-events-none absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-soft)" }}
          >
            <div className="cad-floating-panel flex min-w-[220px] items-center gap-4 px-5 py-4">
              <span className="cad-loader-spinner" aria-hidden="true" />
              <div>
                <p className="cad-kicker">Core Startup</p>
                <p className="mt-2 text-sm text-on-surface-muted">
                  Starting the native CAD core...
                </p>
              </div>
            </div>
          </div>
        ) : null}
        {hasActiveDocument ? (
          <>
            <div className="pointer-events-none absolute bottom-4 right-4 cad-floating-panel px-4 py-3 text-right">
              <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
                Selection
              </p>
              <p className="mt-1 text-sm text-on-surface-muted">
                {selectedReference?.label ??
                  selectedPrimitiveLabel ??
                  "No selection"}
              </p>
              <p className="mt-1 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                {activeSketchPlaneId
                  ? `${activeSketchPlaneId} · ${activeSketchTool} · ${lineCount} line${lineCount === 1 ? "" : "s"} · ${circleCount} circle${circleCount === 1 ? "" : "s"}`
                  : "No active sketch"}
              </p>
              {activeSketchPlaneId ? (
                <p className="mt-1 text-xs text-on-surface-dim">
                  {armedSketchConstraint
                    ? armedSketchConstraint.kind === "coincident"
                      ? armedSketchConstraint.firstPointId
                        ? `Coincident armed · first ${armedSketchConstraint.firstPointId} · click second point`
                        : "Coincident armed · click first point"
                      : armedSketchConstraint.kind === "equal_length" ||
                          armedSketchConstraint.kind === "perpendicular" ||
                          armedSketchConstraint.kind === "parallel"
                        ? armedSketchConstraint.firstLineId
                          ? `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"} armed · first ${armedSketchConstraint.firstLineId} · click second line`
                          : `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"} armed · click first line`
                        : `${armedSketchConstraint.kind} constraint armed · click a line`
                    : document?.selected_sketch_entity_id
                      ? document?.selected_sketch_dimension_id
                        ? `Dimension: ${document.selected_sketch_dimension_id} · Entity: ${document.selected_sketch_entity_id}`
                        : `Entity: ${document.selected_sketch_entity_id}`
                      : document?.selected_sketch_profile_id
                        ? `Profile: ${document.selected_sketch_profile_id}`
                        : sketchSnapLabel
                          ? `Snap: ${sketchSnapLabel}`
                          : activeSketchTool === "select"
                            ? "Selection mode · press a sketch tool to draw"
                            : activeSketchTool === "line" &&
                                lineDraftStartRef.current
                              ? "Line chain active · click to continue or press Escape"
                              : "Click to place geometry"}
                </p>
              ) : null}
            </div>
          </>
        ) : null}
      </div>
    </section>
  );
}
