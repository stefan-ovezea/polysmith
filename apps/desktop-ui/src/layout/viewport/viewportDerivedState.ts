import * as THREE from "three";

import type {
  DocumentState,
  SketchDimensionScene,
  ViewportScene,
  ViewportState,
} from "@/types";
import {
  getSketchGridFrame,
  type ActiveSketchGridPlaneFrame,
} from "./grid";
import {
  buildAngleDimensionFrame,
  circleRadiusDimensionProjection,
} from "./dimensionLabelDrag";
import { clampAngleRadius } from "./draftDimensions";

type SketchParameters = NonNullable<
  DocumentState["feature_history"][number]["sketch_parameters"]
>;

export interface ViewportDerivedStateInput {
  document: DocumentState | null;
  viewport: ViewportState | null;
  sceneData: ViewportScene | null;
  sketchParameters: SketchParameters | null | undefined;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  angleDragRadii: Record<string, number>;
  anglePlacementPreviews: Record<string, SketchDimensionScene>;
  dimensionLabelPositions: Record<string, [number, number, number]>;
}

export function computeViewportDerivedState({
  document,
  viewport,
  sceneData,
  sketchParameters,
  activeSketchPlaneFrame,
  angleDragRadii,
  anglePlacementPreviews,
  dimensionLabelPositions,
}: ViewportDerivedStateInput) {
  const displayedSketchDimensions = computeDisplayedSketchDimensions({
    sceneData,
    sketchParameters,
    activeSketchPlaneFrame,
    angleDragRadii,
    anglePlacementPreviews,
    dimensionLabelPositions,
  });
  const selectedSketchDimensionId =
    document?.selected_sketch_dimension_id ?? null;

  return {
    selectedPrimitiveLabel: selectedPrimitiveLabel(viewport),
    selectedReference:
      viewport?.reference_planes.find(
        (referencePlane) => referencePlane.is_selected,
      ) ?? null,
    measurementText: selectionMeasurementText(document, viewport),
    displayedSketchDimensions,
    selectedSketchDimension: selectedSketchDimensionId
      ? (displayedSketchDimensions.find(
          (dimension) => dimension.dimensionId === selectedSketchDimensionId,
        ) ?? null)
      : null,
    selectedSketchDimensionValue:
      selectedSketchDimensionId && sketchParameters
        ? (sketchParameters.dimensions.find(
            (dimension) => dimension.dimension_id === selectedSketchDimensionId,
          )?.value ?? null)
        : null,
    selectedSketchDimensionExpression:
      selectedSketchDimensionId && sketchParameters
        ? (sketchParameters.dimensions.find(
            (dimension) => dimension.dimension_id === selectedSketchDimensionId,
          )?.expression ?? "")
        : "",
  };
}

function selectedPrimitiveLabel(viewport: ViewportState | null) {
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
}

function selectionMeasurementText(
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  if (!document || !viewport) {
    return null;
  }
  if (document.selected_edge_ids.length === 1) {
    const edge = viewport.edges.find(
      (entry) => entry.id === document.selected_edge_ids[0],
    );
    if (edge) {
      return `Length: ${edge.length.toFixed(2)} mm`;
    }
  }
  if (document.selected_vertex_ids.length === 2) {
    const [aId, bId] = document.selected_vertex_ids;
    const a = viewport.vertices.find((entry) => entry.id === aId);
    const b = viewport.vertices.find((entry) => entry.id === bId);
    if (a && b) {
      const dx = a.position.x - b.position.x;
      const dy = a.position.y - b.position.y;
      const dz = a.position.z - b.position.z;
      const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
      return `Distance: ${distance.toFixed(2)} mm`;
    }
  }
  return null;
}

function computeDisplayedSketchDimensions({
  sceneData,
  sketchParameters,
  activeSketchPlaneFrame,
  angleDragRadii,
  anglePlacementPreviews,
  dimensionLabelPositions,
}: Omit<
  ViewportDerivedStateInput,
  "document" | "viewport"
>): SketchDimensionScene[] {
  if (!sceneData) {
    return [];
  }
  return sceneData.sketchDimensions.map((dimension) =>
    displayedSketchDimension({
      dimension,
      sketchParameters,
      activeSketchPlaneFrame,
      angleDragRadii,
      anglePlacementPreviews,
      dimensionLabelPositions,
    }),
  );
}

function displayedSketchDimension({
  dimension,
  sketchParameters,
  activeSketchPlaneFrame,
  angleDragRadii,
  anglePlacementPreviews,
  dimensionLabelPositions,
}: {
  dimension: SketchDimensionScene;
  sketchParameters: SketchParameters | null | undefined;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  angleDragRadii: Record<string, number>;
  anglePlacementPreviews: Record<string, SketchDimensionScene>;
  dimensionLabelPositions: Record<string, [number, number, number]>;
}): SketchDimensionScene {
  const anglePlacementPreview = anglePlacementPreviews[dimension.dimensionId];
  if (anglePlacementPreview) {
    return anglePlacementPreview;
  }

  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    return displayedAngleDimension({
      dimension,
      sketchParameters,
      angleDragRadii,
    });
  }

  const labelPosition = dimensionLabelPositions[dimension.dimensionId];
  if (!labelPosition) {
    return dimension;
  }
  if (dimension.kind === "circle_radius") {
    const projected = projectedCircleRadiusDimension({
      dimension,
      labelPosition,
      activeSketchPlaneFrame,
    });
    if (projected) {
      return projected;
    }
  }

  return shiftedLinearDimension({
    dimension,
    labelPosition,
    activeSketchPlaneFrame,
  });
}

function displayedAngleDimension({
  dimension,
  sketchParameters,
  angleDragRadii,
}: {
  dimension: SketchDimensionScene;
  sketchParameters: SketchParameters | null | undefined;
  angleDragRadii: Record<string, number>;
}): SketchDimensionScene {
  const dragRadius = angleDragRadii[dimension.dimensionId];
  if (dragRadius === undefined) {
    return dimension;
  }
  const frame = buildAngleDimensionFrame({
    dimension,
    sketchParameters,
  });
  if (!frame) {
    return dimension;
  }
  const radius = clampAngleRadius(dragRadius);
  const toTuple = (point: THREE.Vector3): [number, number, number] => [
    point.x,
    point.y,
    point.z,
  ];
  return {
    ...dimension,
    arcRadius: radius,
    anchorStart: toTuple(
      frame.pivot
        .clone()
        .add(frame.startUnit.clone().multiplyScalar(frame.anchorRadius)),
    ),
    anchorEnd: toTuple(
      frame.pivot
        .clone()
        .add(frame.endUnit.clone().multiplyScalar(frame.anchorRadius)),
    ),
    dimensionStart: toTuple(
      frame.pivot.clone().add(frame.startUnit.clone().multiplyScalar(radius)),
    ),
    dimensionEnd: toTuple(
      frame.pivot.clone().add(frame.endUnit.clone().multiplyScalar(radius)),
    ),
    labelPosition: toTuple(
      frame.pivot.clone().add(frame.bisector.clone().multiplyScalar(radius)),
    ),
  };
}

function projectedCircleRadiusDimension({
  dimension,
  labelPosition,
  activeSketchPlaneFrame,
}: {
  dimension: SketchDimensionScene;
  labelPosition: [number, number, number];
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
}): SketchDimensionScene | null {
  const projection = circleRadiusDimensionProjection({
    dimension,
    worldPoint: labelPosition,
    planeFrame: activeSketchPlaneFrame,
  });
  if (!projection) {
    return null;
  }
  const start = projection.center
    .clone()
    .add(projection.direction.clone().multiplyScalar(-projection.radius));
  const end = projection.center
    .clone()
    .add(projection.direction.clone().multiplyScalar(projection.radius));
  const toTuple = (point: THREE.Vector3): [number, number, number] => [
    point.x,
    point.y,
    point.z,
  ];
  return {
    ...dimension,
    anchorStart: toTuple(start),
    anchorEnd: toTuple(end),
    dimensionStart: toTuple(start),
    dimensionEnd: toTuple(end),
    labelPosition,
  };
}

function shiftedLinearDimension({
  dimension,
  labelPosition,
  activeSketchPlaneFrame,
}: {
  dimension: SketchDimensionScene;
  labelPosition: [number, number, number];
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
}): SketchDimensionScene {
  const originalLabel = new THREE.Vector3(...dimension.labelPosition);
  const nextLabel = new THREE.Vector3(...labelPosition);
  let offset = nextLabel.sub(originalLabel);
  const extensionAxis = new THREE.Vector3(...dimension.dimensionStart).sub(
    new THREE.Vector3(...dimension.anchorStart),
  );
  const dimensionDirection = new THREE.Vector3(...dimension.dimensionEnd).sub(
    new THREE.Vector3(...dimension.dimensionStart),
  );
  const placementAxis =
    extensionAxis.lengthSq() > 1e-8
      ? extensionAxis.normalize()
      : getSketchGridFrame(dimension.planeId, activeSketchPlaneFrame).normal
          .cross(dimensionDirection)
          .normalize();
  if (placementAxis.lengthSq() > 1e-8) {
    offset = placementAxis.multiplyScalar(offset.dot(placementAxis));
  }
  const shiftPoint = (point: [number, number, number]) => {
    const shifted = new THREE.Vector3(...point).add(offset);
    return [shifted.x, shifted.y, shifted.z] as [number, number, number];
  };
  return {
    ...dimension,
    dimensionStart: shiftPoint(dimension.dimensionStart),
    dimensionEnd: shiftPoint(dimension.dimensionEnd),
    labelPosition: shiftPoint(dimension.labelPosition),
  };
}
