import { useEffect, type MutableRefObject } from "react";

import type {
  ArmedSketchConstraint,
  ConstraintType,
  MoveFeatureParameters,
  SketchTool,
  SolidFacePlaneFrame,
} from "@/types";
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import type { MoveGizmoDescriptor } from "./moveGizmo";
import type {
  PolygonToolMode,
  SketchSelection,
  ViewportPanelProps,
} from "./viewportPanelTypes";

interface ViewportCallbackRefTargets {
  selectPrimitiveRef: MutableRefObject<(primitiveId: string) => Promise<void>>;
  selectReferenceRef: MutableRefObject<(referenceId: string) => Promise<void>>;
  selectFaceRef: MutableRefObject<(faceId: string) => Promise<void>>;
  selectEdgeRef: MutableRefObject<
    (edgeId: string, additive: boolean) => Promise<void>
  >;
  selectVertexRef: MutableRefObject<
    (vertexId: string, additive: boolean) => Promise<void>
  >;
  startSketchRef: MutableRefObject<(referenceId: string) => Promise<void>>;
  startSketchOnFaceRef: MutableRefObject<
    (faceId: string, planeFrame: SolidFacePlaneFrame) => Promise<void>
  >;
  setSketchMidpointAnchorRef: MutableRefObject<
    (pointId: string, hostLineId: string) => Promise<void>
  >;
  setSketchPointLineAnchorRef: MutableRefObject<
    (pointId: string, hostLineId: string, t: number) => Promise<void>
  >;
  addSketchLineRef: MutableRefObject<
    (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchRectangleRef: MutableRefObject<
    (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchCircleRef: MutableRefObject<
    (
      centerX: number,
      centerY: number,
      radius: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchCircleModeRef: MutableRefObject<
    (
      mode: string,
      isConstruction: boolean,
      inputs: {
        p1?: [number, number];
        p2?: [number, number];
        p3?: [number, number];
        lineAId?: string;
        lineBId?: string;
        lineCId?: string;
        hint?: [number, number];
      },
    ) => Promise<void>
  >;
  addSketchAngleDimensionRef: MutableRefObject<
    (firstLineId: string, secondLineId: string) => Promise<void>
  >;
  addSketchDistanceDimensionRef: MutableRefObject<
    (firstEntityId: string, secondEntityId: string) => Promise<void>
  >;
  addSketchLineLengthDimensionRef: MutableRefObject<
    (lineId: string) => Promise<void>
  >;
  addSketchLineAngleDimensionRef: MutableRefObject<
    (lineId: string) => Promise<void>
  >;
  addSketchCircleRadiusDimensionRef: MutableRefObject<
    (circleId: string, displayAs?: string) => Promise<void>
  >;
  addSketchArcRadiusDimensionRef: MutableRefObject<
    (arcId: string) => Promise<void>
  >;
  addSketchArcLengthDimensionRef: MutableRefObject<
    (arcId: string) => Promise<void>
  >;
  addSketchPolygonRadiusDimensionRef: MutableRefObject<
    (polygonId: string) => Promise<void>
  >;
  setSketchLineConstraintRef: MutableRefObject<
    (
      lineId: string,
      constraint: "none" | "horizontal" | "vertical",
    ) => Promise<void>
  >;
  setSketchPerpendicularConstraintRef: MutableRefObject<
    (lineId: string, otherLineId: string | null) => Promise<void>
  >;
  setSketchTangentConstraintRef: MutableRefObject<
    (lineId: string, circleId: string) => Promise<void>
  >;
  setSketchParallelConstraintRef: MutableRefObject<
    (lineId: string, otherLineId: string | null) => Promise<void>
  >;
  addSketchArcRef: MutableRefObject<
    (
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      anchorX: number,
      anchorY: number,
      mode: ArcToolMode,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  arcToolModeRef: MutableRefObject<ArcToolMode>;
  rectangleToolModeRef: MutableRefObject<RectangleToolMode>;
  circleToolModeRef: MutableRefObject<CircleToolMode>;
  polygonToolModeRef: MutableRefObject<PolygonToolMode>;
  polygonSidesRef: MutableRefObject<number>;
  addSketchPolygonRef: MutableRefObject<
    (
      sides: number,
      mode: string,
      startX: number,
      startY: number,
      endX: number,
      endY: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchFilletRef: MutableRefObject<
    (
      cornerPointId: string,
      lineAId: string,
      lineBId: string,
    ) => Promise<void>
  >;
  addSketchChamferRef: MutableRefObject<
    (
      cornerPointId: string,
      lineAId: string,
      lineBId: string,
    ) => Promise<void>
  >;
  addSketchEllipseRef: MutableRefObject<
    (
      centerX: number,
      centerY: number,
      axisAX: number,
      axisAY: number,
      axisBX: number,
      axisBY: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchSplineRef: MutableRefObject<
    (
      points: Array<{ x: number; y: number }>,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchSlotRef: MutableRefObject<
    (
      centerX: number,
      centerY: number,
      length: number,
      radius: number,
      rotation: number,
      isConstruction: boolean,
    ) => Promise<void>
  >;
  addSketchTextRef: MutableRefObject<
    (anchorX: number, anchorY: number) => Promise<void>
  >;
  pickSketchTextRef: MutableRefObject<(textId: string) => void>;
  pickSketchSlotRef: MutableRefObject<(slotId: string) => void>;
  pickSketchChamferRef: MutableRefObject<(chamferId: string) => void>;
  extendSketchEntityRef: MutableRefObject<
    (entityId: string, clickX: number, clickY: number) => Promise<void>
  >;
  offsetSketchEntityRef: MutableRefObject<
    (entityId: string) => Promise<void>
  >;
  selectSketchEntityRef: MutableRefObject<
    (entityId: string, additive: boolean) => Promise<void>
  >;
  pickInactiveSketchLineRef: MutableRefObject<
    ((lineId: string) => void | Promise<void>) | undefined
  >;
  inactiveSketchEntityPickEnabledRef: MutableRefObject<boolean>;
  pickSketchPointRef: MutableRefObject<
    (
      pointId: string,
      kind: "endpoint" | "center" | "quadrant",
      additive: boolean,
    ) => Promise<void>
  >;
  updateSketchPointRef: MutableRefObject<
    (pointId: string, x: number, y: number) => Promise<void>
  >;
  moveSketchEntitiesRef: MutableRefObject<
    (params: {
      entityIds: string[];
      dx: number;
      dy: number;
      centerX: number;
      centerY: number;
      angleDeg: number;
    }) => Promise<void>
  >;
  selectSketchDimensionRef: MutableRefObject<
    (dimensionId: string) => Promise<void>
  >;
  updateSketchDimensionRef: MutableRefObject<
    (dimensionId: string, value: number | string) => Promise<void>
  >;
  updateSketchDimensionLabelPositionRef: MutableRefObject<
    (dimensionId: string, labelX: number, labelY: number) => Promise<void>
  >;
  addSketchVertexDistanceDimensionRef: MutableRefObject<
    (pointAId: string, pointBId: string) => Promise<void>
  >;
  updateSketchDimensionDisplayRef: MutableRefObject<
    (dimensionId: string, displayAs: string) => Promise<void>
  >;
  selectSketchProfileRef: MutableRefObject<
    (profileId: string, additive: boolean) => Promise<void>
  >;
  trimSketchEntityRef: MutableRefObject<
    ((entityId: string, clickX: number, clickY: number) => Promise<void>) | undefined
  >;
  deleteSketchSelectionRef: MutableRefObject<
    (selection?: SketchSelection) => Promise<void>
  >;
  setSketchToolRef: MutableRefObject<(tool: SketchTool) => Promise<void>>;
  armedSketchConstraintRef: MutableRefObject<ArmedSketchConstraint>;
  mirrorFocusedSlotRef: MutableRefObject<"objects" | "axis" | null>;
  mirrorEntityPickRef: MutableRefObject<
    (entityId: string, entityKind: "line" | "circle") => Promise<void>
  >;
  cancelSketchConstraintRef: MutableRefObject<() => void>;
  clearSketchConstraintRef: MutableRefObject<
    (
      kind: ConstraintType,
      entityId: string,
      relatedEntityId: string | null,
    ) => Promise<void>
  >;
  moveGizmoRef: MutableRefObject<MoveGizmoDescriptor | null>;
  moveGizmoChangeRef: MutableRefObject<
    ((parameters: MoveFeatureParameters) => Promise<void> | void) | undefined
  >;
  moveBodyRef: MutableRefObject<
    ((bodyId: string) => Promise<void> | void) | undefined
  >;
  copyBodyRef: MutableRefObject<
    ((
      bodyId: string,
      copyMode: "linked" | "standalone",
    ) => Promise<void> | void) | undefined
  >;
  exportBodyMeshRef: MutableRefObject<
    ((bodyId: string) => Promise<void> | void) | undefined
  >;
  unlinkBodyCopyRef: MutableRefObject<
    ((featureId: string) => Promise<void> | void) | undefined
  >;
}

interface ViewportCallbackRefValues
  extends Pick<
    ViewportPanelProps,
    | "onSelectPrimitive"
    | "onSelectReference"
    | "onSelectFace"
    | "onSelectEdge"
    | "onSelectVertex"
    | "onStartSketch"
    | "onStartSketchOnFace"
  > {
  onSetSketchMidpointAnchor: ViewportCallbackRefTargets["setSketchMidpointAnchorRef"]["current"];
  onSetSketchPointLineAnchor: ViewportCallbackRefTargets["setSketchPointLineAnchorRef"]["current"];
  onAddSketchLine: ViewportCallbackRefTargets["addSketchLineRef"]["current"];
  onAddSketchRectangle: ViewportCallbackRefTargets["addSketchRectangleRef"]["current"];
  onAddSketchCircle: ViewportCallbackRefTargets["addSketchCircleRef"]["current"];
  onAddSketchArc: ViewportCallbackRefTargets["addSketchArcRef"]["current"];
  onAddSketchAngleDimension: ViewportCallbackRefTargets["addSketchAngleDimensionRef"]["current"];
  onAddSketchDistanceDimension: ViewportCallbackRefTargets["addSketchDistanceDimensionRef"]["current"];
  onAddSketchLineLengthDimension: ViewportCallbackRefTargets["addSketchLineLengthDimensionRef"]["current"];
  onAddSketchLineAngleDimension: ViewportCallbackRefTargets["addSketchLineAngleDimensionRef"]["current"];
  onAddSketchCircleRadiusDimension: ViewportCallbackRefTargets["addSketchCircleRadiusDimensionRef"]["current"];
  onAddSketchArcRadiusDimension: ViewportCallbackRefTargets["addSketchArcRadiusDimensionRef"]["current"];
  onAddSketchArcLengthDimension: ViewportCallbackRefTargets["addSketchArcLengthDimensionRef"]["current"];
  onAddSketchPolygonRadiusDimension: ViewportCallbackRefTargets["addSketchPolygonRadiusDimensionRef"]["current"];
  onSetSketchLineConstraint: ViewportCallbackRefTargets["setSketchLineConstraintRef"]["current"];
  onSetSketchPerpendicularConstraint: ViewportCallbackRefTargets["setSketchPerpendicularConstraintRef"]["current"];
  onSetSketchTangentConstraint: ViewportCallbackRefTargets["setSketchTangentConstraintRef"]["current"];
  onSetSketchParallelConstraint: ViewportCallbackRefTargets["setSketchParallelConstraintRef"]["current"];
  arcToolMode: ArcToolMode;
  rectangleToolMode: RectangleToolMode;
  circleToolMode: CircleToolMode;
  polygonToolMode: PolygonToolMode;
  polygonSides: number;
  onAddSketchPolygon: ViewportCallbackRefTargets["addSketchPolygonRef"]["current"];
  onAddSketchFillet: ViewportCallbackRefTargets["addSketchFilletRef"]["current"];
  onAddSketchChamfer: ViewportCallbackRefTargets["addSketchChamferRef"]["current"];
  onAddSketchEllipse: ViewportCallbackRefTargets["addSketchEllipseRef"]["current"];
  onAddSketchSpline: ViewportCallbackRefTargets["addSketchSplineRef"]["current"];
  onAddSketchSlot: ViewportCallbackRefTargets["addSketchSlotRef"]["current"];
  onAddSketchText: ViewportCallbackRefTargets["addSketchTextRef"]["current"];
  onPickSketchText: ViewportCallbackRefTargets["pickSketchTextRef"]["current"];
  onPickSketchSlot: ViewportCallbackRefTargets["pickSketchSlotRef"]["current"];
  onPickSketchChamfer: ViewportCallbackRefTargets["pickSketchChamferRef"]["current"];
  onExtendSketchEntity: ViewportCallbackRefTargets["extendSketchEntityRef"]["current"];
  onOffsetSketchEntity: ViewportCallbackRefTargets["offsetSketchEntityRef"]["current"];
  onOpenTransformArray: () => void;
  onAddSketchCircleMode: (
    mode: string,
    isConstruction: boolean,
    inputs: {
      p1?: [number, number];
      p2?: [number, number];
      p3?: [number, number];
      lineAId?: string;
      lineBId?: string;
      lineCId?: string;
      hint?: [number, number];
    },
  ) => Promise<void>;
  onSelectSketchEntity: ViewportCallbackRefTargets["selectSketchEntityRef"]["current"];
  onPickInactiveSketchLine:
    | ViewportCallbackRefTargets["pickInactiveSketchLineRef"]["current"]
    | undefined;
  inactiveSketchEntityPickEnabled: boolean;
  onPickSketchPoint: ViewportCallbackRefTargets["pickSketchPointRef"]["current"];
  onUpdateSketchPoint: ViewportCallbackRefTargets["updateSketchPointRef"]["current"];
  onMoveSketchEntities: ViewportCallbackRefTargets["moveSketchEntitiesRef"]["current"];
  onSelectSketchDimension: ViewportCallbackRefTargets["selectSketchDimensionRef"]["current"];
  onUpdateSketchDimension: ViewportCallbackRefTargets["updateSketchDimensionRef"]["current"];
  onUpdateSketchDimensionLabelPosition: ViewportCallbackRefTargets["updateSketchDimensionLabelPositionRef"]["current"];
  onAddSketchVertexDistanceDimension: ViewportCallbackRefTargets["addSketchVertexDistanceDimensionRef"]["current"];
  onUpdateSketchDimensionDisplay: ViewportCallbackRefTargets["updateSketchDimensionDisplayRef"]["current"];
  onSelectSketchProfile: ViewportCallbackRefTargets["selectSketchProfileRef"]["current"];
  onTrimSketchEntity: ViewportCallbackRefTargets["trimSketchEntityRef"]["current"];
  onDeleteSketchSelection: ViewportCallbackRefTargets["deleteSketchSelectionRef"]["current"];
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  armedSketchConstraint: ArmedSketchConstraint;
  mirrorFocusedSlot: "objects" | "axis" | null;
  onMirrorEntityPick: ViewportCallbackRefTargets["mirrorEntityPickRef"]["current"];
  onCancelSketchConstraint: () => void;
  onClearSketchConstraint: ViewportCallbackRefTargets["clearSketchConstraintRef"]["current"];
  moveGizmo: MoveGizmoDescriptor | null;
  onMoveGizmoChange:
    | ViewportCallbackRefTargets["moveGizmoChangeRef"]["current"]
    | undefined;
  onMoveBody: ViewportCallbackRefTargets["moveBodyRef"]["current"] | undefined;
  onCopyBody: ViewportCallbackRefTargets["copyBodyRef"]["current"] | undefined;
  onExportBodyMesh:
    | ViewportCallbackRefTargets["exportBodyMeshRef"]["current"]
    | undefined;
  onUnlinkBodyCopy:
    | ViewportCallbackRefTargets["unlinkBodyCopyRef"]["current"]
    | undefined;
}

export function useViewportCallbackRefs(
  refs: ViewportCallbackRefTargets,
  values: ViewportCallbackRefValues,
) {
  useEffect(() => {
    refs.selectPrimitiveRef.current = values.onSelectPrimitive;
    refs.selectReferenceRef.current = values.onSelectReference;
    refs.selectFaceRef.current = values.onSelectFace;
    refs.selectEdgeRef.current = values.onSelectEdge;
    refs.selectVertexRef.current = values.onSelectVertex;
    refs.startSketchRef.current = values.onStartSketch;
    refs.startSketchOnFaceRef.current = values.onStartSketchOnFace;
    refs.setSketchMidpointAnchorRef.current = values.onSetSketchMidpointAnchor;
    refs.setSketchPointLineAnchorRef.current =
      values.onSetSketchPointLineAnchor;
    refs.addSketchLineRef.current = values.onAddSketchLine;
    refs.addSketchRectangleRef.current = values.onAddSketchRectangle;
    refs.addSketchCircleRef.current = values.onAddSketchCircle;
    refs.addSketchCircleModeRef.current = values.onAddSketchCircleMode;
    refs.addSketchArcRef.current = values.onAddSketchArc;
    refs.addSketchAngleDimensionRef.current =
      values.onAddSketchAngleDimension;
    refs.addSketchDistanceDimensionRef.current =
      values.onAddSketchDistanceDimension;
    refs.addSketchLineLengthDimensionRef.current =
      values.onAddSketchLineLengthDimension;
    refs.addSketchLineAngleDimensionRef.current =
      values.onAddSketchLineAngleDimension;
    refs.addSketchCircleRadiusDimensionRef.current =
      values.onAddSketchCircleRadiusDimension;
    refs.addSketchArcRadiusDimensionRef.current =
      values.onAddSketchArcRadiusDimension;
    refs.addSketchArcLengthDimensionRef.current =
      values.onAddSketchArcLengthDimension;
    refs.addSketchPolygonRadiusDimensionRef.current =
      values.onAddSketchPolygonRadiusDimension;
    refs.setSketchLineConstraintRef.current = values.onSetSketchLineConstraint;
    refs.setSketchPerpendicularConstraintRef.current =
      values.onSetSketchPerpendicularConstraint;
    refs.setSketchTangentConstraintRef.current =
      values.onSetSketchTangentConstraint;
    refs.setSketchParallelConstraintRef.current =
      values.onSetSketchParallelConstraint;
    refs.arcToolModeRef.current = values.arcToolMode;
    refs.rectangleToolModeRef.current = values.rectangleToolMode;
    refs.circleToolModeRef.current = values.circleToolMode;
    refs.polygonToolModeRef.current = values.polygonToolMode;
    refs.polygonSidesRef.current = values.polygonSides;
    refs.addSketchPolygonRef.current = values.onAddSketchPolygon;
    refs.addSketchFilletRef.current = values.onAddSketchFillet;
    refs.addSketchChamferRef.current = values.onAddSketchChamfer;
    refs.addSketchEllipseRef.current = values.onAddSketchEllipse;
    refs.addSketchSplineRef.current = values.onAddSketchSpline;
    refs.addSketchSlotRef.current = values.onAddSketchSlot;
    refs.addSketchTextRef.current = values.onAddSketchText;
    refs.pickSketchTextRef.current = values.onPickSketchText;
    refs.pickSketchSlotRef.current = values.onPickSketchSlot;
    refs.pickSketchChamferRef.current = values.onPickSketchChamfer;
    refs.extendSketchEntityRef.current = values.onExtendSketchEntity;
    refs.offsetSketchEntityRef.current = values.onOffsetSketchEntity;
    refs.selectSketchEntityRef.current = values.onSelectSketchEntity;
    refs.pickInactiveSketchLineRef.current = values.onPickInactiveSketchLine;
    refs.inactiveSketchEntityPickEnabledRef.current =
      values.inactiveSketchEntityPickEnabled;
    refs.pickSketchPointRef.current = values.onPickSketchPoint;
    refs.updateSketchPointRef.current = values.onUpdateSketchPoint;
    refs.moveSketchEntitiesRef.current = values.onMoveSketchEntities;
    refs.selectSketchDimensionRef.current = values.onSelectSketchDimension;
    refs.updateSketchDimensionRef.current = values.onUpdateSketchDimension;
    refs.updateSketchDimensionLabelPositionRef.current =
      values.onUpdateSketchDimensionLabelPosition;
    refs.addSketchVertexDistanceDimensionRef.current =
      values.onAddSketchVertexDistanceDimension;
    refs.updateSketchDimensionDisplayRef.current =
      values.onUpdateSketchDimensionDisplay;
    refs.selectSketchProfileRef.current = values.onSelectSketchProfile;
    refs.trimSketchEntityRef.current = values.onTrimSketchEntity;
    refs.deleteSketchSelectionRef.current = values.onDeleteSketchSelection;
    refs.setSketchToolRef.current = values.onSetSketchTool;
    refs.armedSketchConstraintRef.current = values.armedSketchConstraint;
    refs.mirrorFocusedSlotRef.current = values.mirrorFocusedSlot;
    refs.mirrorEntityPickRef.current = values.onMirrorEntityPick;
    refs.cancelSketchConstraintRef.current = values.onCancelSketchConstraint;
    refs.clearSketchConstraintRef.current = values.onClearSketchConstraint;
    refs.moveGizmoRef.current = values.moveGizmo;
    refs.moveGizmoChangeRef.current = values.onMoveGizmoChange;
    refs.moveBodyRef.current = values.onMoveBody;
    refs.copyBodyRef.current = values.onCopyBody;
    refs.exportBodyMeshRef.current = values.onExportBodyMesh;
    refs.unlinkBodyCopyRef.current = values.onUnlinkBodyCopy;
  }, [refs, values]);
}
