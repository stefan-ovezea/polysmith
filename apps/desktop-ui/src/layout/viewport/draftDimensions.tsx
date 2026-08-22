import type { SketchTool } from "@/types";
import { distanceBetweenPoints } from "@/utils";

export type DraftDimensionTool = "line" | "rectangle" | "circle" | "polygon" | "arc";
export type DraftDimensionField =
  | "length"
  | "width"
  | "diameter"
  | "radius"
  | "angle";

export type DraftDimensionSession = {
  tool: DraftDimensionTool;
  start: [number, number];
  current: [number, number];
  values: Record<DraftDimensionField, string>;
  activeField: DraftDimensionField;
  lockedFields: Partial<Record<DraftDimensionField, boolean>>;
  // Fields the user has ever typed into during this draft session,
  // even if they later cleared the value. Prevents auto-dimension
  // deletion when the user interacts with a field at all.
  touchedFields: Partial<Record<DraftDimensionField, boolean>>;
};

export type ParameterSuggestion = {
  name: string;
  expression: string;
  kind: "length" | "angle";
  value: number;
};

export type DimensionLabelDragState = {
  dimensionId: string;
  startClientX: number;
  startClientY: number;
  startWorld: [number, number, number];
  startLabelPosition: [number, number, number];
  dragAxis: [number, number, number];
  hasMoved: boolean;
  isPlacement?: boolean;
  hitPart?: "label" | "geometry";
  anglePlacementRelation?: DimensionRelationPreview;
};

export type DimensionRelationPreview = {
  kind:
    | "parallel_line_distance"
    | "line_angle"
    | "circle_line_distance"
    | "circle_center_distance";
  firstEntityId: string;
  targetEntityId: string;
};

const ANGLE_DIMENSION_MIN_RADIUS = 6;
const ANGLE_DIMENSION_MAX_RADIUS = 500;

export const DRAFT_DIMENSION_OFFSET_PX = 36;

export function clampAngleRadius(distance: number): number {
  return Math.max(
    ANGLE_DIMENSION_MIN_RADIUS,
    Math.min(distance, ANGLE_DIMENSION_MAX_RADIUS),
  );
}

export function parameterTokenAtCursor(value: string, cursor: number) {
  const before = value.slice(0, cursor);
  const after = value.slice(cursor);
  const startMatch = before.match(/[A-Za-z_][A-Za-z0-9_]*$/);
  if (!startMatch) {
    return null;
  }
  const endMatch = after.match(/^[A-Za-z0-9_]*/);
  const start = cursor - startMatch[0].length;
  const end = cursor + (endMatch?.[0].length ?? 0);
  return { query: value.slice(start, cursor), start, end };
}

export function fuzzyParameterScore(query: string, candidate: string) {
  const normalizedQuery = query.toLowerCase();
  const normalizedCandidate = candidate.toLowerCase();
  if (!normalizedQuery) {
    return 1;
  }
  if (normalizedCandidate === normalizedQuery) {
    return 1000;
  }
  if (normalizedCandidate.startsWith(normalizedQuery)) {
    return 800 - (normalizedCandidate.length - normalizedQuery.length);
  }
  if (normalizedCandidate.includes(normalizedQuery)) {
    return 600 - normalizedCandidate.indexOf(normalizedQuery);
  }

  let score = 0;
  let candidateIndex = 0;
  let previousMatch = -1;
  for (const char of normalizedQuery) {
    const found = normalizedCandidate.indexOf(char, candidateIndex);
    if (found < 0) {
      return 0;
    }
    score += previousMatch >= 0 && found === previousMatch + 1 ? 12 : 4;
    if (found === 0 || /[_\-\s]/.test(candidate[found - 1] ?? "")) {
      score += 8;
    }
    previousMatch = found;
    candidateIndex = found + 1;
  }
  return score - normalizedCandidate.length * 0.1;
}

export function GridMiniIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 16 16"
      fill="none"
    >
      <path
        d="M3 2.5V13.5M8 2.5V13.5M13 2.5V13.5M2.5 3H13.5M2.5 8H13.5M2.5 13H13.5"
        stroke="currentColor"
        strokeWidth="1.35"
        strokeLinecap="round"
      />
    </svg>
  );
}

export function isDraftDimensionTool(
  tool: SketchTool,
): tool is DraftDimensionTool {
  return (
    tool === "line" ||
    tool === "rectangle" ||
    tool === "circle" ||
    tool === "polygon" ||
    tool === "arc"
  );
}

export function isDrawableSketchTool(
  tool: SketchTool | null,
): tool is DraftDimensionTool | "arc" | "polygon" | "ellipse" | "slot" {
  return (
    tool === "line" ||
    tool === "rectangle" ||
    tool === "circle" ||
    tool === "arc" ||
    tool === "polygon" ||
    tool === "ellipse" ||
    tool === "slot"
  );
}

export function sketchToolLabelKey(
  tool: DraftDimensionTool | "arc" | "polygon" | "ellipse" | "slot",
): string {
  if (tool === "line") {
    return "toolbar.line";
  }
  if (tool === "rectangle") {
    return "toolbar.rectangle";
  }
  if (tool === "circle") {
    return "toolbar.circle";
  }
  if (tool === "arc") {
    return "toolbar.arc";
  }
  if (tool === "ellipse") {
    return "toolbar.ellipse";
  }
  if (tool === "slot") {
    return "toolbar.slot";
  }
  return "toolbar.polygon";
}

export function formatDraftDimension(value: number): string {
  return Math.max(Math.abs(value), 0).toFixed(2);
}

export function draftSessionValues(
  tool: DraftDimensionTool,
  start: [number, number],
  current: [number, number],
): Record<DraftDimensionField, string> {
  const width = current[0] - start[0];
  const length = current[1] - start[1];
  const radius = distanceBetweenPoints(start, current);
  const lineAngleDeg =
    -Math.atan2(current[1] - start[1], current[0] - start[0]) *
    (180 / Math.PI);
  const lineAngle =
    tool === "line" ? Math.abs(lineAngleDeg).toFixed(2) : "0";
  return {
    length:
      tool === "line" || tool === "arc"
        ? formatDraftDimension(radius)
        : formatDraftDimension(length),
    width: formatDraftDimension(width),
    diameter: formatDraftDimension(radius * 2),
    radius: formatDraftDimension(radius),
    angle: lineAngle,
  };
}

export function draftSessionFields(
  tool: DraftDimensionTool,
): DraftDimensionField[] {
  if (tool === "rectangle") {
    return ["width", "length"];
  }
  if (tool === "circle") {
    return ["diameter"];
  }
  if (tool === "polygon") {
    return ["radius"];
  }
  if (tool === "line") {
    return ["length", "angle"];
  }
  if (tool === "arc") {
    return ["length"];
  }
  return ["length"];
}

export function applyDraftDimensionFieldValue(
  session: DraftDimensionSession,
  field: DraftDimensionField,
  rawValue: string,
  lockField = true,
): DraftDimensionSession {
  const numeric = Number(rawValue);
  const nextValues = { ...session.values, [field]: rawValue };
  if (field === "angle") {
    if (!Number.isFinite(numeric)) {
      return {
        ...session,
        values: nextValues,
        activeField: field,
        lockedFields: lockField
          ? { ...session.lockedFields, [field]: true }
          : session.lockedFields,
        touchedFields: { ...session.touchedFields, [field]: true },
      };
    }
  } else if (!Number.isFinite(numeric) || numeric <= 0) {
    return {
      ...session,
      values: nextValues,
      activeField: field,
      lockedFields: lockField
        ? { ...session.lockedFields, [field]: true }
        : session.lockedFields,
      touchedFields: { ...session.touchedFields, [field]: true },
    };
  }

  const dx = session.current[0] - session.start[0];
  const dy = session.current[1] - session.start[1];
  const signX = dx < 0 ? -1 : 1;
  const signY = dy < 0 ? -1 : 1;
  let current = session.current;
  if (field === "angle") {
    const currentRad = Math.atan2(dy, dx);
    const sign = currentRad >= 0 ? 1 : -1;
    const radians = sign * numeric * (Math.PI / 180);
    const currentLength = Math.hypot(dx, dy) || 1;
    const lockedLength = session.lockedFields.length
      ? Number(session.values.length)
      : NaN;
    const useLength =
      Number.isFinite(lockedLength) && lockedLength > 0
        ? lockedLength
        : currentLength;
    current = [
      session.start[0] + Math.cos(radians) * useLength,
      session.start[1] + Math.sin(radians) * useLength,
    ];
  } else if (session.tool === "rectangle") {
    current = [
      field === "width" ? session.start[0] + signX * numeric : current[0],
      field === "length" ? session.start[1] + signY * numeric : current[1],
    ];
  } else if (session.tool === "circle") {
    const radius = numeric / 2;
    const length = Math.hypot(dx, dy) || 1;
    current = [
      session.start[0] + (dx / length) * radius,
      session.start[1] + (dy / length) * radius,
    ];
  } else {
    const length = Math.hypot(dx, dy) || 1;
    current = [
      session.start[0] + (dx / length) * numeric,
      session.start[1] + (dy / length) * numeric,
    ];
  }

  return {
    ...session,
    current,
    values: {
      ...draftSessionValues(session.tool, session.start, current),
      [field]: rawValue,
    },
    activeField: field,
    lockedFields: lockField
      ? { ...session.lockedFields, [field]: true }
      : session.lockedFields,
    touchedFields: { ...session.touchedFields, [field]: true },
  };
}

export function updateDraftSessionCurrent(
  session: DraftDimensionSession,
  current: [number, number],
): DraftDimensionSession {
  let next: DraftDimensionSession = {
    ...session,
    current,
    values: draftSessionValues(session.tool, session.start, current),
  };

  for (const field of draftSessionFields(session.tool)) {
    if (!session.lockedFields[field]) {
      continue;
    }
    const lockedValue = Number(session.values[field]);
    if (field === "angle") {
      if (!Number.isFinite(lockedValue)) {
        next.values[field] = session.values[field];
        continue;
      }
    } else if (!Number.isFinite(lockedValue) || lockedValue <= 0) {
      next.values[field] = session.values[field];
      continue;
    }
    next = applyDraftDimensionFieldValue(
      { ...next, values: { ...next.values, [field]: session.values[field] } },
      field,
      session.values[field],
      false,
    );
  }

  return {
    ...next,
    activeField: session.activeField,
    values: {
      ...next.values,
      ...Object.fromEntries(
        Object.entries(session.lockedFields)
          .filter(([, locked]) => locked)
          .map(([field]) => [
            field,
            session.values[field as DraftDimensionField],
          ]),
      ),
    },
  };
}
