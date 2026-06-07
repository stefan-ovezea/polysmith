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

type Point2d = [number, number];
type LineBodyHost = { lineId: string; t: number };
type PolygonToolMode = "circumscribed" | "inscribed" | "edge";
type MutableRef<T> = { current: T };

interface DraftCommitSketchPoint extends LineCommitSnapPoint {
  local: Point2d;
}

interface RectangleDraftCommitOptions {
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

interface ArcDraftCommitOptions {
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

interface CircleDraftCommitOptions {
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

interface PolygonDraftCommitOptions {
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

interface LineDraftCommitOptions {
  start: Point2d;
  committedEnd: Point2d;
  isConstruction: boolean;
  endHostLineId: string | null;
  endLineBodyHost: LineBodyHost | null;
  snapEndpointHostLineId: string | null | undefined;
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

interface DraftPointerUpCommitOptions {
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
}

function commitRectangleDraft({
  mode,
  start,
  committedEnd,
  secondPoint,
  isConstruction,
  setSecondPoint,
  clearSecondPoint,
  clearDraftStart,
  scheduleDimensionDeletion,
  scheduleDraftDimensionExpressionUpdate,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
  addSketchRectangle,
}: RectangleDraftCommitOptions): void {
  const [startX, startY] = start;

  if (mode === "three_point") {
    if (!secondPoint) {
      setSecondPoint([committedEnd[0], committedEnd[1]]);
      return;
    }

    clearSecondPoint();
    clearDraftStart();
    scheduleDimensionDeletion();
    scheduleDraftDimensionExpressionUpdate();
    clearDraftDimensionSession();
    suppressDimensionEditorAfterSketchCommit();

    const rectangle = rectangleFromThreePoints2d(start, secondPoint, committedEnd);
    if (!rectangle) {
      return;
    }

    const { minX, minY, maxX, maxY } = rectangle.bounds;
    void addSketchRectangle(minX, minY, maxX, maxY, isConstruction);
    return;
  }

  clearDraftStart();
  scheduleDimensionDeletion();
  scheduleDraftDimensionExpressionUpdate();
  clearDraftDimensionSession();
  suppressDimensionEditorAfterSketchCommit();

  const rectStartX =
    mode === "center_point" ? 2 * startX - committedEnd[0] : startX;
  const rectStartY =
    mode === "center_point" ? 2 * startY - committedEnd[1] : startY;
  void addSketchRectangle(
    rectStartX,
    rectStartY,
    committedEnd[0],
    committedEnd[1],
    isConstruction,
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

function commitCircleDraft({
  mode,
  start,
  committedEnd,
  secondPoint,
  fromCircleCount,
  isConstruction,
  setSecondPoint,
  clearSecondPoint,
  clearDraftStart,
  setPendingCircleDimensionPlacement,
  scheduleDimensionDeletion,
  scheduleDraftDimensionExpressionUpdate,
  clearDraftDimensionSession,
  suppressDimensionEditorAfterSketchCommit,
  addSketchCircle,
}: CircleDraftCommitOptions): void {
  const [startX, startY] = start;

  if (mode === "three_point") {
    if (!secondPoint) {
      setSecondPoint([committedEnd[0], committedEnd[1]]);
      return;
    }

    clearSecondPoint();
    clearDraftStart();
    scheduleDimensionDeletion();
    scheduleDraftDimensionExpressionUpdate();
    clearDraftDimensionSession();
    suppressDimensionEditorAfterSketchCommit();

    const circle = circleFromThreePoints2d(start, secondPoint, committedEnd);
    if (!circle) {
      return;
    }

    void addSketchCircle(
      circle.center[0],
      circle.center[1],
      circle.radius,
      isConstruction,
    );
    return;
  }

  if (mode === "tangent_two_lines" || mode === "tangent_three_lines") {
    return;
  }

  clearDraftStart();
  setPendingCircleDimensionPlacement({
    fromCircleCount,
    center: start,
    end: committedEnd,
  });
  scheduleDimensionDeletion();
  scheduleDraftDimensionExpressionUpdate();
  clearDraftDimensionSession();
  suppressDimensionEditorAfterSketchCommit();

  let center: Point2d = start;
  let radius = distanceBetweenPoints(start, committedEnd);
  if (mode === "two_point") {
    center = [(startX + committedEnd[0]) / 2, (startY + committedEnd[1]) / 2];
    radius = distanceBetweenPoints(start, committedEnd) / 2;
  }

  void addSketchCircle(center[0], center[1], radius, isConstruction);
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
  clearDraftStart();
  scheduleDimensionDeletion();
  scheduleDraftDimensionExpressionUpdate();
  clearDraftDimensionSession();
  suppressDimensionEditorAfterSketchCommit();
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

function commitLineDraft({
  start,
  committedEnd,
  isConstruction,
  endHostLineId,
  endLineBodyHost,
  snapEndpointHostLineId,
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
    refs.draftStart.current = committedEnd;

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
      committedEnd,
      committedEnd,
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

  const { endHostLineId, endLineBodyHost } =
    captureLineCommitRelations(sketchPoint);
  commitLineDraft({
    start,
    committedEnd,
    isConstruction,
    endHostLineId,
    endLineBodyHost,
    snapEndpointHostLineId: sketchPoint.snapEndpointHostLineId,
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
