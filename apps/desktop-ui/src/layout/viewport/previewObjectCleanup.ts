import * as THREE from "three";

import { disposeMaterial } from "@/utils";
import { disposeGeometryTreeResources } from "./threeDisposal";
import type { DimensionRelationPreview } from "./draftDimensions";
import type { TrimLineHighlightSegment } from "./trimHoverPreview";

interface MutableRef<T> {
  current: T;
}

interface PreviewDimensionObject {
  line: THREE.Object3D;
  label: THREE.Sprite;
}

interface ViewportPreviewActionsContext {
  sketchGroupRef: MutableRef<THREE.Group | null>;
  dragPreviewLinesRef: MutableRef<THREE.Line[]>;
  previewLineRef: MutableRef<THREE.Line | null>;
  previewCircleRef: MutableRef<THREE.LineLoop | null>;
  previewArcRef: MutableRef<THREE.Line | null>;
  // Slot draft previews are stadium groups (2 lines + 2 arcs).
  previewSlotRef: MutableRef<THREE.Group | null>;
  previewInferenceRef: MutableRef<THREE.Line[]>;
  trimSegmentHighlightRef: MutableRef<THREE.Line | null>;
  trimArcHighlightRef: MutableRef<THREE.Line | null>;
  previewDimensionRef: MutableRef<PreviewDimensionObject | null>;
  dimensionRelationPreviewRef: MutableRef<DimensionRelationPreview | null>;
  dimensionRelationPreviewLabelRef: MutableRef<
    readonly [number, number, number] | null
  >;
  restoreRelationPreviewHiddenDimensions: () => void;
}

export function createViewportPreviewActions({
  sketchGroupRef,
  dragPreviewLinesRef,
  previewLineRef,
  previewCircleRef,
  previewArcRef,
  previewSlotRef,
  previewInferenceRef,
  trimSegmentHighlightRef,
  trimArcHighlightRef,
  previewDimensionRef,
  dimensionRelationPreviewRef,
  dimensionRelationPreviewLabelRef,
  restoreRelationPreviewHiddenDimensions,
}: ViewportPreviewActionsContext) {
  function clearDragPreviewLines() {
    const sketchGroup = sketchGroupRef.current;
    for (const line of dragPreviewLinesRef.current) {
      if (sketchGroup) {
        sketchGroup.remove(line);
      }
      line.geometry.dispose();
      disposeMaterial(line.material);
    }
    dragPreviewLinesRef.current = [];
  }

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

  function clearPreviewArc() {
    const previewArc = previewArcRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewArc || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewArc);
    previewArc.geometry.dispose();
    disposeMaterial(previewArc.material);
    previewArcRef.current = null;
  }

  function clearTrimSegmentHighlight() {
    const highlight = trimSegmentHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!highlight || !sketchGroup) {
      return;
    }
    sketchGroup.remove(highlight);
    highlight.geometry.dispose();
    disposeMaterial(highlight.material);
    trimSegmentHighlightRef.current = null;
  }

  function clearTrimArcHighlight() {
    const highlight = trimArcHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!highlight || !sketchGroup) {
      return;
    }
    sketchGroup.remove(highlight);
    highlight.geometry.dispose();
    disposeMaterial(highlight.material);
    trimArcHighlightRef.current = null;
  }

  function updateTrimSegmentHighlight(
    _lineId: string,
    segments: TrimLineHighlightSegment[],
    hoveredSegmentIndex: number,
  ) {
    clearTrimSegmentHighlight();
    if (hoveredSegmentIndex < 0 || hoveredSegmentIndex >= segments.length) {
      return;
    }
    const segment = segments[hoveredSegmentIndex];
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) {
      return;
    }

    const material = new THREE.LineBasicMaterial({
      color: 0xff3333,
      transparent: true,
      opacity: 0.9,
      linewidth: 3,
      depthTest: false,
    });
    const points = [
      new THREE.Vector3(segment.sx, segment.sy, segment.sz),
      new THREE.Vector3(segment.ex, segment.ey, segment.ez),
    ];
    const geometry = new THREE.BufferGeometry().setFromPoints(points);
    const highlight = new THREE.Line(geometry, material);
    highlight.renderOrder = 8;
    trimSegmentHighlightRef.current = highlight;
    sketchGroup.add(highlight);
  }

  function updateTrimArcHighlight(
    worldPoints: Array<[number, number, number]>,
  ) {
    clearTrimArcHighlight();
    if (worldPoints.length < 2) {
      return;
    }
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) {
      return;
    }
    const material = new THREE.LineBasicMaterial({
      color: 0xff3333,
      transparent: true,
      opacity: 0.9,
      linewidth: 3,
      depthTest: false,
    });
    const geometry = new THREE.BufferGeometry().setFromPoints(
      worldPoints.map(([x, y, z]) => new THREE.Vector3(x, y, z)),
    );
    const highlight = new THREE.Line(geometry, material);
    highlight.renderOrder = 8;
    trimArcHighlightRef.current = highlight;
    sketchGroup.add(highlight);
  }

  function clearPreviewDimension() {
    const previewDimension = previewDimensionRef.current;
    const sketchGroup = sketchGroupRef.current;
    dimensionRelationPreviewRef.current = null;
    dimensionRelationPreviewLabelRef.current = null;
    restoreRelationPreviewHiddenDimensions();
    if (!previewDimension || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewDimension.line);
    sketchGroup.remove(previewDimension.label);
    disposeGeometryTreeResources(previewDimension.line);
    const labelMaterial = previewDimension.label.material;
    if (labelMaterial instanceof THREE.SpriteMaterial) {
      labelMaterial.map?.dispose();
    }
    disposeMaterial(labelMaterial);
    previewDimensionRef.current = null;
  }

  function clearPreviewSlot() {
    const previewSlot = previewSlotRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewSlot || !sketchGroup) {
      return;
    }
    sketchGroup.remove(previewSlot);
    disposeGeometryTreeResources(previewSlot);
    previewSlotRef.current = null;
  }

  function clearPreviewInference() {
    const sketchGroup = sketchGroupRef.current;
    for (const line of previewInferenceRef.current) {
      if (sketchGroup) {
        sketchGroup.remove(line);
      }
      line.geometry.dispose();
      disposeMaterial(line.material);
    }
    previewInferenceRef.current = [];
  }

  return {
    clearDragPreviewLines,
    clearPreviewArc,
    clearPreviewCircle,
    clearPreviewDimension,
    clearPreviewInference,
    clearPreviewLine,
    clearPreviewSlot,
    clearTrimArcHighlight,
    clearTrimSegmentHighlight,
    updateTrimArcHighlight,
    updateTrimSegmentHighlight,
  };
}


// Clears both trim hover overlays (segment + arc). Callers that hold the
// hook-returned clear functions use this from Escape / tool-switch /
// before-commit paths so a stale red trim highlight can never survive.
export function clearTrimHighlights(
  clearTrimSegmentHighlight: () => void,
  clearTrimArcHighlight: () => void,
) {
  clearTrimSegmentHighlight();
  clearTrimArcHighlight();
}
