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
  buildGrblPositionMarker,
  buildGrblToolpathGroup,
  disposeGrblToolpathGroup,
  updateGrblPositionMarker,
  type GrblBed,
  type GrblToolpathObjects,
} from "./grblPreviewScene";

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
  theme: string;
}

export function GrblPreviewViewport({
  info,
  bed,
  executedLines,
  mpos,
  wco,
  theme,
}: GrblPreviewViewportProps) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const bedGroupRef = useRef<THREE.Group | null>(null);
  const toolpathRef = useRef<GrblToolpathObjects | null>(null);
  const markerRef = useRef<THREE.Group | null>(null);
  // Latest values for the progress/content effects to read without
  // re-subscribing to each other's dependencies.
  const infoRef = useRef<GcodeFileInfo | null>(info);
  infoRef.current = info;
  const executedLinesRef = useRef(executedLines);
  executedLinesRef.current = executedLines;
  const wcoRef = useRef<[number, number, number] | null>(wco);
  wcoRef.current = wco;

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

    const marker = buildGrblPositionMarker();
    marker.visible = false;
    scene.add(marker);

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    markerRef.current = marker;

    let frameId: number | null = null;
    const animate = () => {
      controls.update();
      renderer.render(scene, camera);
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
      renderer.dispose();
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      markerRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

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
    </div>
  );
}
