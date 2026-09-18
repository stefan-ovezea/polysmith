import type { SketchTool } from "@/types";
import { distanceBetweenPoints } from "@/utils";
import { computeArcPreviewGeometry } from "./arcDraftPreview";

export type DraftDimensionTool =
  | "line"
  | "rectangle"
  | "circle"
  | "polygon"
  | "arc"
  | "ellipse";
export type DraftDimensionField =
  | "length"
  | "width"
  | "diameter"
  | "radius"
  | "radiusX"
  | "radiusY"
  | "angle";

export type DraftDimensionSession = {
  tool: DraftDimensionTool;
  start: [number, number];
  current: [number, number];
  /** Second defining point for multi-click drafts — the arc-start
   *  point (three_point/center_start_end arcs) or the major-axis
   *  point (ellipse). Null during the draft's first stage. */
  secondPoint: [number, number] | null;
  /** Tool mode where it changes what a field means — the arc modes.
   *  Undefined for tools without modes. */
  toolMode?: string;
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
    tool === "arc" ||
    tool === "ellipse"
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
  secondPoint?: [number, number] | null,
  toolMode?: string,
): Record<DraftDimensionField, string> {
  const width = current[0] - start[0];
  const length = current[1] - start[1];
  const radius = distanceBetweenPoints(start, current);
  const lineAngleDeg =
    -Math.atan2(current[1] - start[1], current[0] - start[0]) *
    (180 / Math.PI);
  const lineAngle =
    tool === "line" ? Math.abs(lineAngleDeg).toFixed(2) : "0";

  // Arc: the length badge shows the distance between the arc ends.
  // Stage 1 (one end placed) it is the aim distance start→cursor;
  // stage 2 it is the chord — start↔secondPoint for three_point
  // (start is one arc end), secondPoint↔current for center_start_end
  // (start is the center, so the ends are the two later points).
  // The radius badge exists from stage 2 only: the circumradius for
  // three_point (the circle through start, secondPoint and current),
  // the center distance start↔secondPoint for center_start_end.
  let arcLength = radius;
  let arcRadius = radius;
  if (tool === "arc" && secondPoint) {
    arcLength =
      toolMode === "center_start_end"
        ? distanceBetweenPoints(secondPoint, current)
        : distanceBetweenPoints(start, secondPoint);
    if (toolMode === "center_start_end") {
      arcRadius = distanceBetweenPoints(start, secondPoint);
    } else {
      const geometry = computeArcPreviewGeometry({
        mode: "three_point",
        start,
        current,
        secondPoint,
      });
      // Collinear points define no circle — the badge hides via its
      // screen position, so an empty string is only a placeholder.
      arcRadius = geometry ? geometry.radius : NaN;
    }
  }

  // Ellipse: stage 1 (no axis point yet) drafts a circle. Stage 2 shows
  // the true ellipse — radiusX is the major-axis half-length, radiusY
  // the cursor's perpendicular distance from the major axis (mirrors
  // ellipseDraftPreview.ts and the core's add_sketch_ellipse).
  let radiusX = radius;
  let radiusY = radius;
  if (tool === "ellipse" && secondPoint) {
    const ax = secondPoint[0] - start[0];
    const ay = secondPoint[1] - start[1];
    const a = Math.hypot(ax, ay);
    if (a > 0.001) {
      const dx = current[0] - start[0];
      const dy = current[1] - start[1];
      radiusX = a;
      radiusY = Math.abs((ax * dy - ay * dx) / a);
    }
  }

  return {
    length:
      tool === "line"
        ? formatDraftDimension(radius)
        : tool === "arc"
          ? formatDraftDimension(arcLength)
          : formatDraftDimension(length),
    width: formatDraftDimension(width),
    diameter: formatDraftDimension(radius * 2),
    radius:
      tool === "arc"
        ? Number.isFinite(arcRadius)
          ? formatDraftDimension(arcRadius)
          : ""
        : formatDraftDimension(radius),
    radiusX: formatDraftDimension(radiusX),
    radiusY: formatDraftDimension(radiusY),
    angle: lineAngle,
  };
}

/** The second defining point re-projected onto the typed radius about
 *  `start`, keeping its direction. The ellipse major-axis end defines
 *  its radius through this second point (not through `current`), so a
 *  typed radius must move the point itself — the entity preview, the
 *  badge position, and the commit all share this helper. Returns null
 *  when there is nothing to adjust. */
export function typedRadiusSecondPoint(
  session: DraftDimensionSession,
  field: "radius" | "radiusX",
): [number, number] | null {
  if (!session.secondPoint) {
    return null;
  }
  const typed = Number(session.values[field]);
  const dx = session.secondPoint[0] - session.start[0];
  const dy = session.secondPoint[1] - session.start[1];
  const length = Math.hypot(dx, dy);
  if (!Number.isFinite(typed) || typed <= 0 || length <= 1e-9) {
    return null;
  }
  return [
    session.start[0] + (dx / length) * typed,
    session.start[1] + (dy / length) * typed,
  ];
}

/** The three_point arc's second end re-projected onto the typed chord
 *  length about `start`, keeping its direction. The chord is defined
 *  by the SECOND click, not by `current`, so a typed length must move
 *  that point itself — the arc preview, the badge position, and the
 *  commit all share this helper. Returns null when there is nothing
 *  to adjust (center_start_end chords move `current` instead — see
 *  applyDraftDimensionFieldValue). */
export function typedChordSecondPoint(
  session: DraftDimensionSession,
): [number, number] | null {
  if (
    session.tool !== "arc" ||
    session.toolMode === "center_start_end" ||
    !session.secondPoint ||
    !session.lockedFields.length
  ) {
    return null;
  }
  const typed = Number(session.values.length);
  const dx = session.secondPoint[0] - session.start[0];
  const dy = session.secondPoint[1] - session.start[1];
  const length = Math.hypot(dx, dy);
  if (!Number.isFinite(typed) || typed <= 0 || length <= 1e-9) {
    return null;
  }
  return [
    session.start[0] + (dx / length) * typed,
    session.start[1] + (dy / length) * typed,
  ];
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
    return ["length", "radius"];
  }
  if (tool === "ellipse") {
    return ["radiusX", "radiusY"];
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
  // A typed arc radius can reshape the SECOND defining point (the
  // center_start_end arc-start point) in addition to `current`.
  let nextSecondPoint: [number, number] | null = session.secondPoint;
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
  } else if (session.tool === "ellipse") {
    if (!session.secondPoint) {
      // Stage 1: the draft is a circle — scale its radius in place.
      const length = Math.hypot(dx, dy) || 1;
      current = [
        session.start[0] + (dx / length) * numeric,
        session.start[1] + (dy / length) * numeric,
      ];
    } else {
      // Stage 2: keep the major-axis direction and the cursor's side
      // of that axis; retarget one half-axis while preserving the
      // other (mirrors how the ellipse preview derives a and b).
      const ax = session.secondPoint[0] - session.start[0];
      const ay = session.secondPoint[1] - session.start[1];
      const a = Math.hypot(ax, ay);
      if (a > 0.001) {
        const ux = ax / a;
        const uy = ay / a;
        const px = -uy;
        const py = ux;
        const along = dx * ux + dy * uy;
        const perp = dx * px + dy * py;
        const side = perp < 0 ? -1 : 1;
        const absPerp = Math.abs(perp);
        if (field === "radiusX") {
          current = [
            session.start[0] + ux * numeric + px * side * absPerp,
            session.start[1] + uy * numeric + py * side * absPerp,
          ];
        } else {
          current = [
            session.start[0] + ux * along + px * side * numeric,
            session.start[1] + uy * along + py * side * numeric,
          ];
        }
      }
    }
  } else if (session.tool === "arc") {
    if (field === "radius" && session.secondPoint) {
      const [sx, sy] = session.start;
      const [bx, by] = session.secondPoint;
      const chord = Math.hypot(bx - sx, by - sy);
      if (session.toolMode === "center_start_end") {
        // The radius is set by the arc-start point (secondPoint):
        // scale it radially about the center and re-clamp the end
        // onto the new circle.
        if (chord > 1e-9) {
          const k = numeric / chord;
          nextSecondPoint = [sx + (bx - sx) * k, sy + (by - sy) * k];
          const elen = Math.hypot(dx, dy);
          if (elen > 1e-9) {
            current = [
              sx + (dx / elen) * numeric,
              sy + (dy / elen) * numeric,
            ];
          }
        }
      } else if (chord > 1e-9) {
        // three_point: move the apex along the chord's perpendicular
        // bisector until the circumradius equals the typed value
        // (R = (c² + 4d²) / 8d → apex distance from the chord is
        // d = R + sqrt(R² - c²/4), the far solution), staying on the
        // apex's current side of the chord. A radius smaller than
        // half the chord is geometrically impossible (the ends are
        // fixed) — clamp to the closest possible arc: the semicircle.
        // The draft must always respond to the typed value, never
        // silently ignore it.
        const clamped = Math.max(numeric, chord / 2);
        const d =
          clamped + Math.sqrt(clamped * clamped - (chord * chord) / 4);
        const ux = (bx - sx) / chord;
        const uy = (by - sy) / chord;
        const side = (bx - sx) * dy - (by - sy) * dx >= 0 ? 1 : -1;
        current = [
          (sx + bx) / 2 - uy * side * d,
          (sy + by) / 2 + ux * side * d,
        ];
      }
    } else if (!session.secondPoint) {
      // Stage 1 (one end placed): the typed length moves the other
      // end (current) to that distance from the start — same shape
      // as the line length field. With no aim direction yet (the
      // cursor still at the first click, or snapped onto it) the
      // value must still execute instantly — default the direction
      // to +X, like the line tool's angle field treats a degenerate
      // angle as 0°.
      const length = Math.hypot(dx, dy);
      const ux = length > 1e-9 ? dx / length : 1;
      const uy = length > 1e-9 ? dy / length : 0;
      current = [
        session.start[0] + ux * numeric,
        session.start[1] + uy * numeric,
      ];
    } else if (session.toolMode === "center_start_end") {
      // Stage 2: the chord between the placed ends (secondPoint →
      // current) equals the typed length. The radius (start = center
      // → secondPoint) is fixed, so rotate the end around the center
      // until the chord matches.
      const ax = session.secondPoint[0] - session.start[0];
      const ay = session.secondPoint[1] - session.start[1];
      const r = Math.hypot(ax, ay);
      const blen = Math.hypot(dx, dy);
      if (r > 1e-9 && blen > 1e-9 && numeric <= 2 * r) {
        const half = Math.asin(numeric / (2 * r));
        const ux = ax / r;
        const uy = ay / r;
        const vx = dx / blen;
        const vy = dy / blen;
        const side = ux * vy - uy * vx >= 0 ? 1 : -1;
        const theta = side * 2 * half;
        const cosT = Math.cos(theta);
        const sinT = Math.sin(theta);
        current = [
          session.start[0] + r * (ux * cosT - uy * sinT),
          session.start[1] + r * (ux * sinT + uy * cosT),
        ];
      }
      // A chord longer than the diameter is geometrically impossible
      // — keep the stored value, leave the end where it is.
    } else {
      // three_point stage 2: the typed length re-projects the second
      // end (secondPoint) along the chord direction — see
      // typedChordSecondPoint, applied by every consumer (preview,
      // badge position, commit). The anchor (current) stays free.
      current = session.current;
    }
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
    secondPoint: nextSecondPoint,
    values: {
      ...draftSessionValues(
        session.tool,
        session.start,
        current,
        nextSecondPoint,
        session.toolMode,
      ),
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
    values: draftSessionValues(
      session.tool,
      session.start,
      current,
      session.secondPoint,
      session.toolMode,
    ),
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
