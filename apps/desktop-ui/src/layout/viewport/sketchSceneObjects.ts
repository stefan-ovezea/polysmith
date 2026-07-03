import * as THREE from "three";

import type {
  SketchDimensionScene,
  SketchPlaneFrame,
  SketchProfileInteractionState,
  SketchProfileVisual,
  ViewportScene,
} from "@/types";
import {
  buildSketchArcObject,
  buildSketchCircleObject,
  buildSketchConstraintObject,
  buildSketchDimensionObject,
  buildSketchLineObject,
  buildSketchPointObject,
  buildSketchPolygonObject,
  buildSketchProfileObject,
} from "@/utils";

type SelectedSketchConstraint = {
  entityId: string;
  kind: string;
};

export function addSketchSceneObjects({
  sceneData,
  displayedSketchDimensions,
  displayUnits,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  hiddenRelationPreviewDimensionIds,
  selectedConstraint,
  sketchGroup,
  sketchEntityObjects,
  sketchEntityObjectById,
  sketchDimensionObjects,
  dimensionObjectById,
  sketchConstraintObjects,
  sketchProfileObjects,
  sketchProfileVisuals,
  sketchProfileStates,
  sketchPointObjects,
  sketchPointObjectById,
}: {
  sceneData: ViewportScene;
  displayedSketchDimensions: readonly SketchDimensionScene[];
  displayUnits?: "mm" | "in";
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  hiddenRelationPreviewDimensionIds: ReadonlySet<string>;
  selectedConstraint: SelectedSketchConstraint | null;
  sketchGroup: THREE.Group;
  sketchEntityObjects: Array<THREE.Line | THREE.LineLoop>;
  sketchEntityObjectById: Map<string, THREE.Line | THREE.LineLoop>;
  sketchDimensionObjects: THREE.Object3D[];
  dimensionObjectById: Map<string, { line: THREE.Group; label: THREE.Sprite }>;
  sketchConstraintObjects: THREE.Object3D[];
  sketchProfileObjects: THREE.Group[];
  sketchProfileVisuals: Map<string, SketchProfileVisual>;
  sketchProfileStates: Map<string, SketchProfileInteractionState>;
  sketchPointObjects: THREE.Mesh[];
  sketchPointObjectById: Map<string, THREE.Mesh>;
}) {
  addSketchEntityObjects({
    sceneData,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
    sketchGroup,
    sketchEntityObjects,
    sketchEntityObjectById,
  });

  addSketchDimensionObjects({
    displayedSketchDimensions,
    displayUnits,
    hiddenRelationPreviewDimensionIds,
    sketchGroup,
    sketchDimensionObjects,
    dimensionObjectById,
  });

  addSketchConstraintObjects({
    sceneData,
    selectedConstraint,
    sketchGroup,
    sketchConstraintObjects,
  });

  addSketchProfileObjects({
    sceneData,
    sketchGroup,
    sketchProfileObjects,
    sketchProfileVisuals,
    sketchProfileStates,
  });

  addSketchPointObjects({
    sceneData,
    sketchGroup,
    sketchPointObjects,
    sketchPointObjectById,
  });
}

function addSketchEntityObjects({
  sceneData,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketchGroup,
  sketchEntityObjects,
  sketchEntityObjectById,
}: {
  sceneData: ViewportScene;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  sketchGroup: THREE.Group;
  sketchEntityObjects: Array<THREE.Line | THREE.LineLoop>;
  sketchEntityObjectById: Map<string, THREE.Line | THREE.LineLoop>;
}) {
  for (const sketchLine of sceneData.sketchLines) {
    const sketchLineObject = buildSketchLineObject(sketchLine);
    sketchLineObject.userData.isSelected = sketchLine.isSelected;
    sketchEntityObjects.push(sketchLineObject);
    sketchEntityObjectById.set(sketchLine.lineId, sketchLineObject);
    sketchGroup.add(sketchLineObject);
  }

  for (const sketchCircle of sceneData.sketchCircles) {
    const frame =
      sketchCircle.planeFrame ??
      (activeSketchPlaneId &&
      sketchCircle.planeId === activeSketchPlaneId &&
      activeSketchPlaneFrame
        ? activeSketchPlaneFrame
        : null);
    const sketchCircleObject = buildSketchCircleObject(sketchCircle, frame);
    sketchCircleObject.userData.isSelected = sketchCircle.isSelected;
    sketchEntityObjects.push(sketchCircleObject);
    sketchEntityObjectById.set(sketchCircle.circleId, sketchCircleObject);
    sketchGroup.add(sketchCircleObject);
  }

  for (const sketchPolygon of sceneData.sketchPolygons) {
    const sketchPolygonObject = buildSketchPolygonObject(sketchPolygon);
    sketchPolygonObject.userData.isSelected = sketchPolygon.isSelected;
    sketchEntityObjects.push(sketchPolygonObject);
    sketchEntityObjectById.set(sketchPolygon.polygonId, sketchPolygonObject);
    sketchGroup.add(sketchPolygonObject);
  }

  for (const sketchArc of sceneData.sketchArcs) {
    const frame =
      sketchArc.planeFrame ??
      (activeSketchPlaneId &&
      sketchArc.planeId === activeSketchPlaneId &&
      activeSketchPlaneFrame
        ? activeSketchPlaneFrame
        : null);
    const sketchArcObject = buildSketchArcObject(sketchArc, frame);
    sketchArcObject.userData.isSelected = sketchArc.isSelected;
    sketchEntityObjects.push(sketchArcObject);
    sketchEntityObjectById.set(sketchArc.arcId, sketchArcObject);
    sketchGroup.add(sketchArcObject);
  }
}

function addSketchDimensionObjects({
  displayedSketchDimensions,
  displayUnits,
  hiddenRelationPreviewDimensionIds,
  sketchGroup,
  sketchDimensionObjects,
  dimensionObjectById,
}: {
  displayedSketchDimensions: readonly SketchDimensionScene[];
  displayUnits?: "mm" | "in";
  hiddenRelationPreviewDimensionIds: ReadonlySet<string>;
  sketchGroup: THREE.Group;
  sketchDimensionObjects: THREE.Object3D[];
  dimensionObjectById: Map<string, { line: THREE.Group; label: THREE.Sprite }>;
}) {
  dimensionObjectById.clear();
  for (const sketchDimension of displayedSketchDimensions) {
    const sketchDimensionObject = buildSketchDimensionObject(
      sketchDimension,
      displayUnits,
    );
    if (hiddenRelationPreviewDimensionIds.has(sketchDimension.dimensionId)) {
      sketchDimensionObject.line.visible = false;
      sketchDimensionObject.label.visible = false;
    }
    sketchDimensionObjects.push(sketchDimensionObject.line);
    sketchDimensionObjects.push(sketchDimensionObject.label);
    sketchGroup.add(sketchDimensionObject.line);
    sketchGroup.add(sketchDimensionObject.label);
    dimensionObjectById.set(sketchDimension.dimensionId, {
      line: sketchDimensionObject.line as THREE.Group,
      label: sketchDimensionObject.label,
    });
  }
}

function addSketchConstraintObjects({
  sceneData,
  selectedConstraint,
  sketchGroup,
  sketchConstraintObjects,
}: {
  sceneData: ViewportScene;
  selectedConstraint: SelectedSketchConstraint | null;
  sketchGroup: THREE.Group;
  sketchConstraintObjects: THREE.Object3D[];
}) {
  for (const sketchConstraint of sceneData.sketchConstraints) {
    if (sketchConstraint.kind === "fixed") {
      continue;
    }
    const sketchConstraintObject =
      buildSketchConstraintObject(sketchConstraint);
    sketchConstraintObjects.push(sketchConstraintObject);
    sketchGroup.add(sketchConstraintObject);
  }

  for (const object of sketchConstraintObjects) {
    const constraintEntityId =
      object.userData.sketchConstraintEntityId as string | undefined;
    const constraintKind =
      object.userData.sketchConstraintKind as string | undefined;
    const isSelected =
      selectedConstraint !== null &&
      constraintEntityId === selectedConstraint.entityId &&
      constraintKind === selectedConstraint.kind;
    if (
      object instanceof THREE.Sprite &&
      object.material instanceof THREE.SpriteMaterial
    ) {
      if (isSelected) {
        object.material.color.set(0x60e0ff);
        object.scale.set(7.5, 7.5, 1);
      } else {
        object.material.color.set(0xffffff);
        object.scale.set(6, 6, 1);
      }
    }
  }
}

function addSketchProfileObjects({
  sceneData,
  sketchGroup,
  sketchProfileObjects,
  sketchProfileVisuals,
  sketchProfileStates,
}: {
  sceneData: ViewportScene;
  sketchGroup: THREE.Group;
  sketchProfileObjects: THREE.Group[];
  sketchProfileVisuals: Map<string, SketchProfileVisual>;
  sketchProfileStates: Map<string, SketchProfileInteractionState>;
}) {
  for (const sketchProfile of sceneData.sketchProfiles) {
    const sketchProfileObject = buildSketchProfileObject(sketchProfile);
    sketchProfileObjects.push(sketchProfileObject.group);
    sketchProfileVisuals.set(
      sketchProfile.profileId,
      sketchProfileObject.visual,
    );
    sketchProfileStates.set(sketchProfile.profileId, {
      isSelected: sketchProfile.isSelected,
      isHovered: false,
    });
    sketchGroup.add(sketchProfileObject.group);
  }
}

function addSketchPointObjects({
  sceneData,
  sketchGroup,
  sketchPointObjects,
  sketchPointObjectById,
}: {
  sceneData: ViewportScene;
  sketchGroup: THREE.Group;
  sketchPointObjects: THREE.Mesh[];
  sketchPointObjectById: Map<string, THREE.Mesh>;
}) {
  for (const sketchPoint of sceneData.sketchPoints) {
    if (sketchPoint.kind === "quadrant") {
      continue;
    }
    const sketchPointObject = buildSketchPointObject(sketchPoint);
    sketchPointObject.userData.isSelected = sketchPoint.isSelected;
    sketchPointObjects.push(sketchPointObject);
    sketchPointObjectById.set(sketchPoint.pointId, sketchPointObject);
    sketchGroup.add(sketchPointObject);
  }
}
