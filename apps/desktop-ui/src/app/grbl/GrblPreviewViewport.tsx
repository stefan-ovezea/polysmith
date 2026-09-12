import { useEffect, useRef } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import {
  configureViewportControls,
  handleViewportWheelZoom,
  resizeViewportRenderer,
} from "@/layout/viewport/viewportRenderer";
import {
  ORTHO_FRUSTUM_HEIGHT,
  ORTHO_MAX_ZOOM,
  ORTHO_MIN_ZOOM,
  WHEEL_ZOOM_POINTER_PAN,
  WHEEL_ZOOM_SPEED,
} from "@/layout/viewport/viewportPanelTypes";
import type { GcodeFileInfo } from "@/lib/grblClient";
import { disposeGroup } from "@/utils";
import { frameCamera } from "@/utils/viewport/viewportMath";
import {
  applyGrblProgress,
  buildGrblBedGroup,
  buildGrblOverlayToolpathGroup,
  buildGrblPointerMarker,
  buildGrblPositionMarker,
  buildGrblToolpathGroup,
  disposeGrblToolpathGroup,
  updateGrblPointerMarker,
  updateGrblPositionMarker,
  type GrblBed,
  type GrblToolpathObjects,
} from "./grblPreviewScene";
import {
  buildGrblOrientationCube,
  type GrblOrientationCube,
} from "./grblOrientationCube";

// Self-contained Three.js viewport for the GRBL toolpath preview.  It
// owns its canvas/renderer/camera/controls (the CAD ViewportPanel
// monolith is not reusable here) but reuses the shared viewport
// helpers: control configuration, wheel zoom, resizing, and camera
// framing.  One rAF render loop for the whole mount.
interface GrblPreviewViewportProps {
  info: GcodeFileInfo | null;
  bed: GrblBed;
  executedLines: number;
  /** Machine position (MCS) — the crosshair overlays the shifted
   *  toolpath 1:1. */
  mpos: [number, number, number] | null;
  /** WCS offset (WCO = MPos − WPos) — shifts the toolpath from file
   *  space into machine space, so "Zero XY" moves the job onto the
   *  crosshair like LightBurn's origin handling. */
  wco: [number, number, number] | null;
  /** Red laser pointer: XY offset from the focal point (machine
   *  definition) and whether the user turned the dot on. */
  pointerOffset: [number, number] | null;
  pointerOn: boolean;
  /** Utility program overlay (framing/focus pulse) drawn on top of
   *  the real toolpath with its own highlight count. */
  overlayInfo: GcodeFileInfo | null;
  overlayExecutedLines: number;
  theme: string;
}

export function GrblPreviewViewport({
  info,
  bed,
  executedLines,
  mpos,
  wco,
  pointerOffset,
  pointerOn,
  overlayInfo,
  overlayExecutedLines,
  theme,
}: GrblPreviewViewportProps) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const cubeCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const cubeRef = useRef<GrblOrientationCube | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const bedGroupRef = useRef<THREE.Group | null>(null);
  const toolpathRef = useRef<GrblToolpathObjects | null>(null);
  const markerRef = useRef<THREE.Group | null>(null);
  const pointerMarkerRef = useRef<THREE.Group | null>(null);
  const overlayRef = useRef<GrblToolpathObjects | null>(null);
  // Latest values for the progress/content effects to read without
  // re-subscribing to each other's dependencies.
  const infoRef = useRef<GcodeFileInfo | null>(info);
  infoRef.current = info;
  const executedLinesRef = useRef(executedLines);
  executedLinesRef.current = executedLines;
  const wcoRef = useRef<[number, number, number] | null>(wco);
  wcoRef.current = wco;
  const overlayInfoRef = useRef<GcodeFileInfo | null>(overlayInfo);
  overlayInfoRef.current = overlayInfo;
  const overlayExecutedLinesRef = useRef(overlayExecutedLines);
  overlayExecutedLinesRef.current = overlayExecutedLines;

  // One-time renderer/scene/camera setup + render loop (mount only).
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
    renderer.setPixelRatio(window.devicePixelRatio);
    const scene = new THREE.Scene();
    const camera = new THREE.OrthographicCamera(
      -ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      -ORTHO_FRUSTUM_HEIGHT / 2,
      -10000,
      10000,
    );
    camera.up.set(0, 0, 1);
    const controls = new OrbitControls(camera, renderer.domElement);
    configureViewportControls({ controls, canvas });
    // The shared config reserves both mouse buttons (CAD picking
    // semantics) — this viewport has nothing to pick, so the left
    // drag rotates the grid and the right drag pans it.  Together
    // with the orientation cube this makes the view fully orientable.
    controls.mouseButtons.LEFT = THREE.MOUSE.ROTATE;
    controls.mouseButtons.RIGHT = THREE.MOUSE.PAN;

    const marker = buildGrblPositionMarker();
    marker.visible = false;
    scene.add(marker);

    const pointerMarker = buildGrblPointerMarker();
    pointerMarker.visible = false;
    scene.add(pointerMarker);

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    markerRef.current = marker;
    pointerMarkerRef.current = pointerMarker;

    let frameId: number | null = null;
    const animate = () => {
      controls.update();
      renderer.render(scene, camera);
      // The orientation cube mirrors the camera every frame (the
      // widget owns its renderer; the ref indirection lets theme
      // changes rebuild it without restarting this loop).
      cubeRef.current?.update(camera, controls);
      frameId = requestAnimationFrame(animate);
    };
    frameId = requestAnimationFrame(animate);

    const onWheel = (event: WheelEvent) => {
      handleViewportWheelZoom({
        event,
        renderer,
        camera,
        controls,
        minZoom: ORTHO_MIN_ZOOM,
        maxZoom: ORTHO_MAX_ZOOM,
        zoomSpeed: WHEEL_ZOOM_SPEED,
        pointerPan: WHEEL_ZOOM_POINTER_PAN,
      });
    };
    host.addEventListener("wheel", onWheel, { passive: false });

    const onResize = () => {
      resizeViewportRenderer({
        host,
        renderer,
        camera,
        orthoFrustumHeight: ORTHO_FRUSTUM_HEIGHT,
        // The size only feeds the CAD viewport's state; this viewport
        // doesn't render size-dependent HTML.
        setViewportSize: () => {},
      });
    };
    const observer = new ResizeObserver(onResize);
    observer.observe(host);
    onResize();

    return () => {
      if (frameId !== null) {
        cancelAnimationFrame(frameId);
      }
      observer.disconnect();
      host.removeEventListener("wheel", onWheel);
      controls.dispose();
      disposeGroup(marker);
      disposeGroup(pointerMarker);
      renderer.dispose();
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      markerRef.current = null;
      pointerMarkerRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Orientation cube: built once the main camera/controls exist and
  // rebuilt on theme change (its face colors come from CSS tokens).
  useEffect(() => {
    const canvas = cubeCanvasRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    if (!canvas || !camera || !controls) {
      return undefined;
    }
    cubeRef.current = buildGrblOrientationCube({ canvas, mainCamera: camera, controls });
    return () => {
      cubeRef.current?.dispose();
      cubeRef.current = null;
    };
  }, [theme]);

  // Rebuild bed + toolpath when the file, bed, or theme changes.
  useEffect(() => {
    const scene = sceneRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    if (!scene || !camera || !controls) {
      return undefined;
    }

    // disposeGroup (shared util) requires a non-null group — guard the
    // first run, when nothing has been built yet.
    if (bedGroupRef.current) {
      disposeGroup(bedGroupRef.current);
    }
    disposeGrblToolpathGroup(toolpathRef.current);
    bedGroupRef.current = null;
    toolpathRef.current = null;

    const bedGroup = buildGrblBedGroup(bed, infoRef.current?.bounds ?? null);
    bedGroupRef.current = bedGroup;
    scene.add(bedGroup);

    if (infoRef.current) {
      const toolpath = buildGrblToolpathGroup(infoRef.current);
      toolpathRef.current = toolpath;
      // Machine space: shift the file coordinates by the WCS offset
      // so the toolpath sits where the machine will actually cut and
      // the MPos crosshair overlays it.
      const offset = wcoRef.current ?? [0, 0, 0];
      toolpath.group.position.set(offset[0], offset[1], offset[2]);
      scene.add(toolpath.group);
      applyGrblProgress(
        infoRef.current,
        toolpath.lineObjects,
        toolpath.materials,
        executedLinesRef.current,
      );
    }

    // Frame the toolpath when a file is loaded (the export can exceed
    // the configured bed — machine settings trail the real machine),
    // else the bed center. The CAD isometric default view
    // (frameCamera) keeps X/Y recognisable.
    const bounds = infoRef.current?.bounds ?? null;
    const offset = wcoRef.current ?? [0, 0, 0];
    const center: [number, number, number] = bounds
      ? [
          (bounds.minX + bounds.maxX) / 2 + offset[0],
          (bounds.minY + bounds.maxY) / 2 + offset[1],
          0,
        ]
      : [bed.widthMm / 2, bed.heightMm / 2, 0];
    const maxDimension = bounds
      ? Math.max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY)
      : Math.max(bed.widthMm, bed.heightMm);
    frameCamera(camera, controls, center, maxDimension);

    return () => {
      if (bedGroupRef.current) {
        disposeGroup(bedGroupRef.current);
      }
      disposeGrblToolpathGroup(toolpathRef.current);
      bedGroupRef.current = null;
      toolpathRef.current = null;
    };
  }, [info, bed, theme]);

  // Highlight executed lines as the stream progresses.
  useEffect(() => {
    const infoNow = infoRef.current;
    const toolpath = toolpathRef.current;
    if (!infoNow || !toolpath) {
      return;
    }
    applyGrblProgress(
      infoNow,
      toolpath.lineObjects,
      toolpath.materials,
      executedLines,
    );
  }, [executedLines]);

  // Utility overlay (framing/focus pulse): built separately from the
  // main toolpath so it can appear and clear without touching the job.
  useEffect(() => {
    const scene = sceneRef.current;
    if (!scene) {
      return;
    }
    disposeGrblToolpathGroup(overlayRef.current);
    overlayRef.current = null;
    const overlay = overlayInfoRef.current;
    if (!overlay) {
      return;
    }
    const objects = buildGrblOverlayToolpathGroup(overlay);
    const offset = wcoRef.current ?? [0, 0, 0];
    objects.group.position.set(offset[0], offset[1], offset[2]);
    overlayRef.current = objects;
    scene.add(objects.group);
    applyGrblProgress(
      overlay,
      objects.lineObjects,
      objects.materials,
      overlayExecutedLinesRef.current,
    );
    return () => {
      disposeGrblToolpathGroup(overlayRef.current);
      overlayRef.current = null;
    };
  }, [overlayInfo]);

  // Overlay progress, tracked independently from the main toolpath so
  // a utility run never mis-highlights the job (and vice versa).
  useEffect(() => {
    const overlay = overlayRef.current;
    const overlayInfoNow = overlayInfoRef.current;
    if (!overlay || !overlayInfoNow) {
      return;
    }
    applyGrblProgress(
      overlayInfoNow,
      overlay.lineObjects,
      overlay.materials,
      overlayExecutedLines,
    );
  }, [overlayExecutedLines]);

  // Live red laser pointer: MPos + the machine's pointer offset.
  useEffect(() => {
    const pointerMarker = pointerMarkerRef.current;
    if (!pointerMarker) {
      return;
    }
    if (pointerOn && mpos && pointerOffset) {
      updateGrblPointerMarker(pointerMarker, [
        mpos[0] + pointerOffset[0],
        mpos[1] + pointerOffset[1],
        mpos[2],
      ]);
    } else {
      pointerMarker.visible = false;
    }
  }, [pointerOn, mpos, pointerOffset]);

  // Live WCS-offset updates: re-zeroing moves the whole toolpath onto
  // the machine position without rebuilding it.
  useEffect(() => {
    const toolpath = toolpathRef.current;
    if (!toolpath || !wco) {
      return;
    }
    toolpath.group.position.set(wco[0], wco[1], wco[2]);
  }, [wco]);

  // Live machine crosshair from GRBL MPos.
  useEffect(() => {
    const marker = markerRef.current;
    if (!marker) {
      return;
    }
    if (mpos) {
      updateGrblPositionMarker(marker, mpos);
    } else {
      marker.visible = false;
    }
  }, [mpos]);

  return (
    <div ref={hostRef} className="relative h-full w-full overflow-hidden">
      <canvas ref={canvasRef} className="block h-full w-full" />
      {/* Orientation cube overlay: click a face to snap the view
          (top/front/right/…), drag the main view to rotate. */}
      <canvas
        ref={cubeCanvasRef}
        aria-label="Orientation cube"
        className="pointer-events-auto absolute right-3 top-3 h-24 w-24 cursor-pointer"
      />
    </div>
  );
}
