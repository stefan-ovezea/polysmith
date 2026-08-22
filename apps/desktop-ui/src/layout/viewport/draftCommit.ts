import {
  circleFromThreePoints2d,
  distanceBetweenPoints,
  rectangleFromThreePoints2d,
} from "@/utils";
import type { SketchTool } from "@/types";
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type {
  DraftDimensionField,
  DraftDimensionSession,
} from "./draftDimensions";
import { isDraftDimensionTool } from "./draftDimensions";
import {
  draftStartRelations,
  type LineCommitSnapPoint,
} from "./lineCommitRelations";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import { defaultSlotRadius } from "./slotDraftPreview";

export type Point2d = [number, number];
export type LineBodyHost = { lineId: string; t: number };
export type PolygonToolMode = "circumscribed" | "inscribed" | "edge";
type MutableRef<T> = { current: T };

export interface DraftCommitSketchPoint extends LineCommitSnapPoint {
  local: Point2d;
}

interface DraftFinalizeActions {
  clearDraftStart: () => void;
  scheduleDimensionDeletion: () => void;
  scheduleDraftDimensionExpressionUpdate: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
}

interface ThreePointDraftActions extends DraftFinalizeActions {
  committedEnd: Point2d;
  secondPoint: Point2d | null;
  setSecondPoint: (point: Point2d) => void;
  clearSecondPoint: () => void;
}

function finalizeDraftCommit({
  clearDraftStart,
  scheduleDimensionDeletion,
  scheduleDraftDimensionExpressionUpdate,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
}: DraftFinalizeActions) {
  clearDraftStart();
  scheduleDimensionDeletion();
  scheduleDraftDimensionExpressionUpdate();
  clearDraftDimensionSession();
  suppressDimensionEditorAfterSketchCommit();
}

function prepareThreePointDraft(options: ThreePointDraftActions) {
  if (!options.secondPoint) {
    options.setSecondPoint([options.committedEnd[0], options.committedEnd[1]]);
    return null;
  }

  options.clearSecondPoint();
  finalizeDraftCommit(options);
  return options.secondPoint;
}

export interface RectangleDraftCommitOptions {
  mode: RectangleToolMode;
  start: Point2d;
  committedEnd: Point2d;
  secondPoint: Point2d | null;
  isConstruction: boolean;
  setSecondPoint: (point: Point2d) => void;
  clearSecondPoint: () => void;
  clearDraftStart: () => void;
  scheduleDimensionDeletion: () => void;
  scheduleDraftDimensionExpressionUpdate: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  addSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface ArcDraftCommitOptions {
  mode: ArcToolMode;
  start: Point2d;
  current: Point2d;
  secondPoint: Point2d | null;
  isConstruction: boolean;
  setSecondPoint: (point: Point2d) => void;
  clearSecondPoint: () => void;
  clearDraftStart: () => void;
  addSketchArc: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    anchorX: number,
    anchorY: number,
    mode: ArcToolMode,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface CircleDraftCommitOptions {
  mode: CircleToolMode;
  start: Point2d;
  committedEnd: Point2d;
  secondPoint: Point2d | null;
  fromCircleCount: number;
  isConstruction: boolean;
  setSecondPoint: (point: Point2d) => void;
  clearSecondPoint: () => void;
  clearDraftStart: () => void;
  setPendingCircleDimensionPlacement: (placement: {
    fromCircleCount: number;
    center: Point2d;
    end: Point2d;
  }) => void;
  scheduleDimensionDeletion: () => void;
  scheduleDraftDimensionExpressionUpdate: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  addSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface PolygonDraftCommitOptions {
  sides: number;
  mode: PolygonToolMode;
  start: Point2d;
  committedEnd: Point2d;
  isConstruction: boolean;
  clearDraftStart: () => void;
  scheduleDimensionDeletion: () => void;
  scheduleDraftDimensionExpressionUpdate: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  addSketchPolygon: (
    sides: number,
    mode: PolygonToolMode,
    centerX: number,
    centerY: number,
    edgeX: number,
    edgeY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface LineDraftCommitOptions {
  start: Point2d;
  committedEnd: Point2d;
  isConstruction: boolean;
  endHostLineId: string | null;
  endLineBodyHost: LineBodyHost | null;
  snapEndpointHostLineId: string | null | undefined;
  /** Exact tangent contact point from snap resolution. When set,
   *  the next polyline segment starts from this point instead of
   *  `committedEnd`, because the C++ tangent enforcer will move
   *  the line's endpoint to this exact point during the solve. */
  endTangentPoint: Point2d | null | undefined;
  refs: {
    chainBreakRequested: MutableRef<boolean>;
    draftStart: MutableRef<Point2d | null>;
    previousLineAngle: MutableRef<number | null>;
    draftStartMidpointHost: MutableRef<string | null>;
    draftStartEndpointHost: MutableRef<string | null>;
    draftStartLineBodyHost: MutableRef<LineBodyHost | null>;
    draftDimensionSession: MutableRef<DraftDimensionSession | null>;
    draftDimensionInputs: MutableRef<
      Partial<Record<DraftDimensionField, HTMLInputElement | null>>
    >;
  };
  scheduleDraftDimensionExpressionUpdate: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  createDraftDimensionSession: (
    start: Point2d,
    current: Point2d,
  ) => DraftDimensionSession;
  clearDraftDimGroup: () => void;
  setDraftDimensionSession: (session: DraftDimensionSession) => void;
  focusDraftField: (field: DraftDimensionSession["activeField"]) => void;
  scheduleDimensionDeletion: (
    preCapturedSession?: DraftDimensionSession | null,
  ) => void;
  addSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface EllipseDraftCommitOptions {
  start: Point2d;
  current: Point2d;
  axisPoint: Point2d | null;
  isConstruction: boolean;
  setAxisPoint: (point: Point2d) => void;
  clearAxisPoint: () => void;
  clearDraftStart: () => void;
  addSketchEllipse: (
    centerX: number,
    centerY: number,
    axisAX: number,
    axisAY: number,
    axisBX: number,
    axisBY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface SlotDraftCommitOptions {
  start: Point2d;
  committedEnd: Point2d;
  isConstruction: boolean;
  clearDraftStart: () => void;
  clearDraftDimensionSession: () => void;
  addSketchSlot: (
    centerX: number,
    centerY: number,
    length: number,
    radius: number,
    rotation: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
}

export interface DraftPointerUpCommitOptions {
  activeSketchTool: SketchTool;
  sketchPoint: DraftCommitSketchPoint;
  draftDimensionSession: DraftDimensionSession | null;
  sketchCircleCount: number;
  isConstruction: boolean;
  refs: {
    draftStart: MutableRef<Point2d | null>;
    arcSecondPoint: MutableRef<Point2d | null>;
    rectSecondPoint: MutableRef<Point2d | null>;
    circleSecondPoint: MutableRef<Point2d | null>;
    ellipseSecondPoint: MutableRef<Point2d | null>;
    chainBreakRequested: MutableRef<boolean>;
    previousLineAngle: MutableRef<number | null>;
    draftStartMidpointHost: MutableRef<string | null>;
    draftStartEndpointHost: MutableRef<string | null>;
    draftStartLineBodyHost: MutableRef<LineBodyHost | null>;
    draftDimensionSession: MutableRef<DraftDimensionSession | null>;
    draftDimensionInputs: MutableRef<
      Partial<Record<DraftDimensionField, HTMLInputElement | null>>
    >;
  };
  modes: {
    arc: ArcToolMode;
    rectangle: RectangleToolMode;
    circle: CircleToolMode;
    polygon: PolygonToolMode;
  };
  polygonSides: number;
  clearPreviews: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  scheduleDimensionDeletion: (
    tool: "line" | "rectangle" | "circle" | "polygon",
    preCapturedSession?: DraftDimensionSession | null,
  ) => void;
  scheduleDraftDimensionExpressionUpdate: (
    tool: "line" | "rectangle" | "circle" | "polygon",
  ) => void;
  setPendingCircleDimensionPlacement: (placement: {
    fromCircleCount: number;
    center: Point2d;
    end: Point2d;
  }) => void;
  captureLineCommitRelations: (sketchPoint: DraftCommitSketchPoint) => {
    endHostLineId: string | null;
    endLineBodyHost: LineBodyHost | null;
  };
  createLineDraftDimensionSession: (
    start: Point2d,
    current: Point2d,
  ) => DraftDimensionSession;
  clearDraftDimGroup: () => void;
  setDraftDimensionSession: (session: DraftDimensionSession) => void;
  focusDraftField: (field: DraftDimensionSession["activeField"]) => void;
  addSketchArc: ArcDraftCommitOptions["addSketchArc"];
  addSketchRectangle: RectangleDraftCommitOptions["addSketchRectangle"];
  addSketchCircle: CircleDraftCommitOptions["addSketchCircle"];
  addSketchPolygon: PolygonDraftCommitOptions["addSketchPolygon"];
  addSketchLine: LineDraftCommitOptions["addSketchLine"];
  addSketchEllipse: EllipseDraftCommitOptions["addSketchEllipse"];
  addSketchSlot: SlotDraftCommitOptions["addSketchSlot"];
}

function commitRectangleDraft(options: RectangleDraftCommitOptions): void {
  const [startX, startY] = options.start;

  if (options.mode === "three_point") {
    const secondPoint = prepareThreePointDraft(options);
    if (!secondPoint) {
      return;
    }

    const rectangle = rectangleFromThreePoints2d(
      options.start,
      secondPoint,
      options.committedEnd,
    );
    if (!rectangle) {
      return;
    }

    const { minX, minY, maxX, maxY } = rectangle.bounds;
    void options.addSketchRectangle(
      minX,
      minY,
      maxX,
      maxY,
      options.isConstruction,
    );
    return;
  }

  finalizeDraftCommit(options);

  const rectStartX =
    options.mode === "center_point" ? 2 * startX - options.committedEnd[0] : startX;
  const rectStartY =
    options.mode === "center_point" ? 2 * startY - options.committedEnd[1] : startY;
  void options.addSketchRectangle(
    rectStartX,
    rectStartY,
    options.committedEnd[0],
    options.committedEnd[1],
    options.isConstruction,
  );
}

function commitArcDraft({
  mode,
  start,
  current,
  secondPoint,
  isConstruction,
  setSecondPoint,
  clearSecondPoint,
  clearDraftStart,
  addSketchArc,
}: ArcDraftCommitOptions): void {
  if (!secondPoint) {
    setSecondPoint(current);
    return;
  }

  clearSecondPoint();
  clearDraftStart();

  if (mode === "three_point") {
    void addSketchArc(
      start[0],
      start[1],
      secondPoint[0],
      secondPoint[1],
      current[0],
      current[1],
      mode,
      isConstruction,
    );
    return;
  }

  void addSketchArc(
    secondPoint[0],
    secondPoint[1],
    current[0],
    current[1],
    start[0],
    start[1],
    mode,
    isConstruction,
  );
}

function commitCircleDraft(options: CircleDraftCommitOptions): void {
  const [startX, startY] = options.start;

  if (options.mode === "three_point") {
    const secondPoint = prepareThreePointDraft(options);
    if (!secondPoint) {
      return;
    }

    const circle = circleFromThreePoints2d(
      options.start,
      secondPoint,
      options.committedEnd,
    );
    if (!circle) {
      return;
    }

    void options.addSketchCircle(
      circle.center[0],
      circle.center[1],
      circle.radius,
      options.isConstruction,
    );
    return;
  }

  if (
    options.mode === "tangent_two_lines" ||
    options.mode === "tangent_three_lines"
  ) {
    return;
  }

  options.clearDraftStart();
  options.setPendingCircleDimensionPlacement({
    fromCircleCount: options.fromCircleCount,
    center: options.start,
    end: options.committedEnd,
  });
  options.scheduleDimensionDeletion();
  options.scheduleDraftDimensionExpressionUpdate();
  options.clearDraftDimensionSession();
  options.suppressDimensionEditorAfterSketchCommit();

  let center: Point2d = options.start;
  let radius = distanceBetweenPoints(options.start, options.committedEnd);
  if (options.mode === "two_point") {
    center = [
      (startX + options.committedEnd[0]) / 2,
      (startY + options.committedEnd[1]) / 2,
    ];
    radius = distanceBetweenPoints(options.start, options.committedEnd) / 2;
  }

  void options.addSketchCircle(center[0], center[1], radius, options.isConstruction);
}

function commitPolygonDraft({
  sides,
  mode,
  start,
  committedEnd,
  isConstruction,
  clearDraftStart,
  scheduleDimensionDeletion,
  scheduleDraftDimensionExpressionUpdate,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
  addSketchPolygon,
}: PolygonDraftCommitOptions): void {
  finalizeDraftCommit({
    clearDraftStart,
    scheduleDimensionDeletion,
    scheduleDraftDimensionExpressionUpdate,
    clearDraftDimensionSession,
    suppressDimensionEditorAfterSketchCommit,
  });
  void addSketchPolygon(
    sides,
    mode,
    start[0],
    start[1],
    committedEnd[0],
    committedEnd[1],
    isConstruction,
  );
}

function commitEllipseDraft({
  start,
  current,
  axisPoint,
  isConstruction,
  setAxisPoint,
  clearAxisPoint,
  clearDraftStart,
  addSketchEllipse,
}: EllipseDraftCommitOptions): void {
  if (!axisPoint) {
    // First stage done — lock the major-axis point; the next click
    // lands the minor-axis point and commits.
    setAxisPoint([current[0], current[1]]);
    return;
  }

  const ax = axisPoint[0] - start[0];
  const ay = axisPoint[1] - start[1];
  const a = Math.hypot(ax, ay);
  const dx = current[0] - start[0];
  const dy = current[1] - start[1];
  const b = Math.abs((ax * dy - ay * dx) / a);
  if (a <= 0.001 || b <= 0.001) {
    // Degenerate minor axis (cursor on the major-axis line) — the
    // core would reject it; stay in stage 2 instead of failing.
    return;
  }

  clearAxisPoint();
  clearDraftStart();
  void addSketchEllipse(
    start[0],
    start[1],
    axisPoint[0],
    axisPoint[1],
    current[0],
    current[1],
    isConstruction,
  );
}

function commitSlotDraft({
  start,
  committedEnd,
  isConstruction,
  clearDraftStart,
  clearDraftDimensionSession,
  addSketchSlot,
}: SlotDraftCommitOptions): void {
  const dx = committedEnd[0] - start[0];
  const dy = committedEnd[1] - start[1];
  const half = Math.hypot(dx, dy);
  if (half <= 0.001) {
    return;
  }
  const length = 2 * half;
  const rotation = Math.atan2(dy, dx);
  clearDraftStart();
  clearDraftDimensionSession();
  void addSketchSlot(
    start[0],
    start[1],
    length,
    defaultSlotRadius(length),
    rotation,
    isConstruction,
  );
}

function commitLineDraft({
  start,
  committedEnd,
  isConstruction,
  endHostLineId,
  endLineBodyHost,
  snapEndpointHostLineId,
  endTangentPoint,
  refs,
  scheduleDraftDimensionExpressionUpdate,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
  createDraftDimensionSession,
  clearDraftDimGroup,
  setDraftDimensionSession,
  focusDraftField,
  scheduleDimensionDeletion,
  addSketchLine,
}: LineDraftCommitOptions): void {
  if (refs.chainBreakRequested.current) {
    refs.chainBreakRequested.current = false;
    refs.draftStart.current = null;
    refs.previousLineAngle.current = null;
    scheduleDraftDimensionExpressionUpdate();
    clearDraftDimensionSession();
  } else {
    // When the end has a tangent constraint, the C++ core's
    // enforce_tangent_line_circle_relations will move the endpoint
    // to the exact tangent contact point during the solve. Both the
    // next draft start AND the next line's dimension session must
    // use that resolved position so the polyline chain stays
    // watertight and dimension values are computed from the correct
    // origin.
    const nextDraftStart = endTangentPoint ?? committedEnd;
    refs.draftStart.current = nextDraftStart;

    const dx = committedEnd[0] - start[0];
    const dy = committedEnd[1] - start[1];
    if (Math.hypot(dx, dy) > 1e-6) {
      refs.previousLineAngle.current = Math.atan2(dy, dx);
    }

    refs.draftStartMidpointHost.current = endHostLineId;
    refs.draftStartEndpointHost.current = snapEndpointHostLineId ?? null;
    refs.draftStartLineBodyHost.current = endLineBodyHost;
    Object.values(refs.draftDimensionInputs.current).forEach((input) => {
      input?.blur();
    });
    suppressDimensionEditorAfterSketchCommit();

    scheduleDraftDimensionExpressionUpdate();
    const oldSession = refs.draftDimensionSession.current;
    const nextLineSession = createDraftDimensionSession(
      nextDraftStart,
      nextDraftStart,
    );
    clearDraftDimGroup();
    refs.draftDimensionSession.current = nextLineSession;
    setDraftDimensionSession(nextLineSession);
    focusDraftField(nextLineSession.activeField);
    scheduleDimensionDeletion(oldSession);
  }

  void addSketchLine(
    start[0],
    start[1],
    committedEnd[0],
    committedEnd[1],
    isConstruction,
  );
}

export function commitDraftPointerUp({
  activeSketchTool,
  sketchPoint,
  draftDimensionSession,
  sketchCircleCount,
  isConstruction,
  refs,
  modes,
  polygonSides,
  clearPreviews,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
  scheduleDimensionDeletion,
  scheduleDraftDimensionExpressionUpdate,
  setPendingCircleDimensionPlacement,
  captureLineCommitRelations,
  createLineDraftDimensionSession,
  clearDraftDimGroup,
  setDraftDimensionSession,
  focusDraftField,
  addSketchArc,
  addSketchRectangle,
  addSketchCircle,
  addSketchPolygon,
  addSketchLine,
  addSketchEllipse,
  addSketchSlot,
}: DraftPointerUpCommitOptions): void {
  if (!refs.draftStart.current) {
    refs.draftStart.current = sketchPoint.local;
    const startRelations = draftStartRelations(sketchPoint);
    refs.draftStartMidpointHost.current = startRelations.midpointHostLineId;
    refs.draftStartEndpointHost.current = startRelations.endpointHostLineId;
    refs.draftStartLineBodyHost.current = startRelations.lineBodyHost;
    return;
  }

  const start = refs.draftStart.current;
  const committedEnd =
    draftDimensionSession && isDraftDimensionTool(activeSketchTool)
      ? draftDimensionSession.current
      : sketchPoint.local;

  if (
    Math.abs(committedEnd[0] - start[0]) < 0.01 &&
    Math.abs(committedEnd[1] - start[1]) < 0.01
  ) {
    refs.draftStart.current = null;
    clearDraftDimensionSession();
    return;
  }

  clearPreviews();

  if (activeSketchTool === "arc") {
    commitArcDraft({
      mode: modes.arc,
      start,
      current: sketchPoint.local,
      secondPoint: refs.arcSecondPoint.current,
      isConstruction,
      setSecondPoint: (point) => {
        refs.arcSecondPoint.current = point;
      },
      clearSecondPoint: () => {
        refs.arcSecondPoint.current = null;
      },
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      addSketchArc,
    });
    return;
  }

  if (activeSketchTool === "rectangle") {
    commitRectangleDraft({
      mode: modes.rectangle,
      start,
      committedEnd,
      secondPoint: refs.rectSecondPoint.current,
      isConstruction,
      setSecondPoint: (point) => {
        refs.rectSecondPoint.current = point;
      },
      clearSecondPoint: () => {
        refs.rectSecondPoint.current = null;
      },
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      scheduleDimensionDeletion: () => {
        scheduleDimensionDeletion("rectangle");
      },
      scheduleDraftDimensionExpressionUpdate: () => {
        scheduleDraftDimensionExpressionUpdate("rectangle");
      },
      clearDraftDimensionSession,
      suppressDimensionEditorAfterSketchCommit,
      addSketchRectangle,
    });
    return;
  }

  if (activeSketchTool === "circle") {
    commitCircleDraft({
      mode: modes.circle,
      start,
      committedEnd,
      secondPoint: refs.circleSecondPoint.current,
      fromCircleCount: sketchCircleCount,
      isConstruction,
      setSecondPoint: (point) => {
        refs.circleSecondPoint.current = point;
      },
      clearSecondPoint: () => {
        refs.circleSecondPoint.current = null;
      },
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      setPendingCircleDimensionPlacement,
      scheduleDimensionDeletion: () => {
        scheduleDimensionDeletion("circle");
      },
      scheduleDraftDimensionExpressionUpdate: () => {
        scheduleDraftDimensionExpressionUpdate("circle");
      },
      clearDraftDimensionSession,
      suppressDimensionEditorAfterSketchCommit,
      addSketchCircle,
    });
    return;
  }

  if (activeSketchTool === "polygon") {
    commitPolygonDraft({
      sides: polygonSides,
      mode: modes.polygon,
      start,
      committedEnd,
      isConstruction,
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      scheduleDimensionDeletion: () => {
        scheduleDimensionDeletion("polygon");
      },
      scheduleDraftDimensionExpressionUpdate: () => {
        scheduleDraftDimensionExpressionUpdate("polygon");
      },
      clearDraftDimensionSession,
      suppressDimensionEditorAfterSketchCommit,
      addSketchPolygon,
    });
    return;
  }

  if (activeSketchTool === "ellipse") {
    commitEllipseDraft({
      start,
      current: sketchPoint.local,
      axisPoint: refs.ellipseSecondPoint.current,
      isConstruction,
      setAxisPoint: (point) => {
        refs.ellipseSecondPoint.current = point;
      },
      clearAxisPoint: () => {
        refs.ellipseSecondPoint.current = null;
      },
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      addSketchEllipse,
    });
    return;
  }

  if (activeSketchTool === "slot") {
    commitSlotDraft({
      start,
      committedEnd,
      isConstruction,
      clearDraftStart: () => {
        refs.draftStart.current = null;
      },
      clearDraftDimensionSession,
      addSketchSlot,
    });
    return;
  }

  const { endHostLineId, endLineBodyHost } =
    captureLineCommitRelations(sketchPoint);
  commitLineDraft({
    start,
    committedEnd,
    isConstruction,
    endHostLineId,
    endLineBodyHost,
    snapEndpointHostLineId: sketchPoint.snapEndpointHostLineId,
    endTangentPoint: sketchPoint.snapTangentPoint,
    refs,
    scheduleDraftDimensionExpressionUpdate: () => {
      scheduleDraftDimensionExpressionUpdate("line");
    },
    clearDraftDimensionSession,
    suppressDimensionEditorAfterSketchCommit,
    createDraftDimensionSession: createLineDraftDimensionSession,
    clearDraftDimGroup,
    setDraftDimensionSession,
    focusDraftField,
    scheduleDimensionDeletion: (session) => {
      scheduleDimensionDeletion("line", session);
    },
    addSketchLine,
  });
}
