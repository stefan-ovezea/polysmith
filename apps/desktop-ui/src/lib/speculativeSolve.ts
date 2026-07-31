/**
 * speculativeSolve.ts — Solver-based dynamic snap inference.
 *
 * Replaces hand-coded geometric snap math in snapResolution.ts with
 * speculative WASM solver calls. The core insight: instead of computing
 * tangent/perpendicular/parallel snap points with fragile geometry code,
 * push a temporary constraint to the planegcs solver and read the exact
 * snap point directly.
 *
 * Supports both single-constraint (speculativeSolve) and multi-constraint
 * (speculativeMultiSolve) speculative queries. Multi-constraint solves
 * enable compound snaps like line-line intersection and tangent-through-line.
 *
 * Usage (from snapResolution.ts dynamicSnapCandidate):
 *
 *   import { speculativeSolve } from "./speculativeSolve";
 *   const bridge = getBridge();
 *   if (bridge && draftStart) {
 *     const result = speculativeSolve(bridge, {
 *       params, constraints, draftStart, cursor,
 *       snapType: "tangent_lc",
 *       targetEntityId: circle.circle_id,
 *     });
 *     if (result?.converged && result.distance < threshold) {
 *       return { local: result.position, ... };
 *     }
 *   }
 */

import type { PlanegcsBridge, SketchConstraintData } from "@/lib/planegcsBridge";
import type { SketchFeatureParameters } from "@/types/geometry/sketch";

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface SpeculativeSolveParams {
  /** The bridge instance (pass from caller — getBridge()). */
  bridge: PlanegcsBridge;
  /** Current sketch parameters. */
  params: SketchFeatureParameters;
  /** Existing constraints from the sketch / viewport state. */
  constraints: SketchConstraintData[];
  /** Draft start position in sketch-local coordinates [x, y]. */
  draftStart: [number, number];
  /** Raw cursor position in sketch-local coordinates [x, y]. */
  cursor: [number, number];
  /** The speculative constraint kind to test. */
  snapType: SpeculativeSnapType;
  /** Entity ID of the snap target (line_id, circle_id, etc.). */
  targetEntityId: string;
}

/** A single speculative constraint entry for multi-constraint solves. */
export interface SpeculativeConstraintEntry {
  snapType: SpeculativeSnapType;
  targetEntityId: string;
}

/** Parameters for multi-constraint speculative solves (intersection, etc.). */
export interface SpeculativeMultiSolveParams {
  bridge: PlanegcsBridge;
  params: SketchFeatureParameters;
  constraints: SketchConstraintData[];
  draftStart: [number, number];
  cursor: [number, number];
  /** Array of speculative constraints to push simultaneously.
   *  All are marked temporary:true. */
  snapConstraints: SpeculativeConstraintEntry[];
}

export interface SpeculativeSolveResult {
  /** Solved snap position in sketch-local coordinates [x, y]. */
  position: [number, number];
  /** True if the solver converged (status 0 or 1). */
  converged: boolean;
  /** Screen-space distance from raw cursor to solved position. */
  distance: number;
  /** Solver status code (0=Success, 1=Converged, 2=Failed, …). */
  solverStatus: number;
}

/** Snap types that can be resolved via speculative WASM constraints. */
export type SpeculativeSnapType =
  | "tangent_lc"
  | "perpendicular_ll"
  | "parallel"
  | "point_on_line_pl"
  | "horizontal_l"
  | "vertical_l";

// ---------------------------------------------------------------------------
// Virtual primitive IDs (constant — reused across calls)
// ---------------------------------------------------------------------------

const VIRTUAL_DRAFT_POINT_ID = "__spec_draft_start";
const VIRTUAL_CURSOR_POINT_ID = "__spec_cursor";
const VIRTUAL_LINE_ID = "__spec_line";
const SPECULATIVE_CONSTRAINT_ID = "__spec_constraint";

// ---------------------------------------------------------------------------
// Shared geometry + constraint builder
// ---------------------------------------------------------------------------

/**
 * Push all sketch geometry (points, lines, circles) and existing constraints
 * into the WASM solver wrapper. Extracted so both single and multi-constraint
 * speculative solve paths share the same build logic without duplication.
 *
 * Returns the number of sketch points pushed (used later to locate the
 * virtual cursor point in the flat parameter array).
 */
function pushSketchGeometryAndConstraints(
  w: any,
  params: SketchFeatureParameters,
  constraints: SketchConstraintData[],
): number {
  w.clear_data();

  const vertexIds: string[] = [];

  // Push sketch points — ALL fixed. The speculative solver must NOT move
  // existing geometry (circles, lines, endpoints). Only the virtual cursor
  // is free; the solver satisfies constraints by moving the cursor alone.
  for (const pt of params.vertices) {
    const vtxId = pt.vertex_id;
    w.push_primitive({
      id: vtxId,
      type: "point",
      x: pt.x,
      y: pt.y,
      fixed: true,
    });
    vertexIds.push(vtxId);
  }

  // Push sketch lines.
  for (const line of params.lines) {
    w.push_primitive({
      id: line.line_id,
      type: "line",
      p1_id: line.start_vertex_id,
      p2_id: line.end_vertex_id,
    });
  }

  // Push sketch circles.
  for (const circle of params.circles) {
    // Fall back to legacy point-circle-*-center format when the
    // vertex-N center_vertex_id is empty (matches planegcsBridge.ts).
    const centerId = circle.center_vertex_id || `point-circle-${circle.circle_id}-center`;
    w.push_primitive({
      id: circle.circle_id,
      type: "circle",
      c_id: centerId,
      radius: circle.radius,
    });
  }

  // Inline H/V constraints on lines.
  for (const line of params.lines) {
    if (line.constraint === "horizontal") {
      w.push_primitive({
        id: `c-h-${line.line_id}`,
        type: "horizontal_l",
        l_id: line.line_id,
      });
    } else if (line.constraint === "vertical") {
      w.push_primitive({
        id: `c-v-${line.line_id}`,
        type: "vertical_l",
        l_id: line.line_id,
      });
    }
  }

  // Coincident / concentric constraints.
  for (const c of constraints) {
    if (c.kind === "coincident" && c.target_ids.length >= 2) {
      w.push_primitive({
        id: c.constraint_id,
        type: "p2p_coincident",
        p1_id: c.target_ids[0],
        p2_id: c.target_ids[1],
      });
    } else if (c.kind === "concentric" && c.target_ids.length >= 2) {
      const cid1 = `point-circle-${c.target_ids[0]}-center`;
      const cid2 = `point-circle-${c.target_ids[1]}-center`;
      w.push_primitive({
        id: c.constraint_id,
        type: "p2p_coincident",
        p1_id: cid1,
        p2_id: cid2,
      });
    }
  }

  // Line relations.
  for (const rel of params.line_relations) {
    const kind = rel.kind as string;
    switch (kind) {
      case "parallel":
        w.push_primitive({
          id: rel.relation_id,
          type: "parallel",
          l1_id: rel.first_line_id,
          l2_id: rel.second_line_id,
        });
        break;
      case "perpendicular":
        w.push_primitive({
          id: rel.relation_id,
          type: "perpendicular_ll",
          l1_id: rel.first_line_id,
          l2_id: rel.second_line_id,
        });
        break;
      case "equal_length":
        w.push_primitive({
          id: rel.relation_id,
          type: "equal_length",
          l1_id: rel.first_line_id,
          l2_id: rel.second_line_id,
        });
        break;
      case "tangent_line_circle":
        w.push_primitive({
          id: rel.relation_id,
          type: "tangent_lc",
          l_id: rel.first_line_id,
          c_id: rel.second_line_id,
        });
        break;
    }
  }

  // Midpoint and point-line anchors.
  for (const a of params.midpoint_anchors) {
    w.push_primitive({
      id: a.anchor_id,
      type: "point_on_line_pl",
      p_id: a.vertex_id,
      l_id: a.line_id,
    });
  }
  for (const a of params.point_line_anchors) {
    w.push_primitive({
      id: a.anchor_id,
      type: "point_on_line_pl",
      p_id: a.vertex_id,
      l_id: a.line_id,
    });
  }

  // Driving dimensions.
  for (const dim of params.dimensions) {
    if (dim.driven) continue;
    if (dim.expression.length === 0) continue;

    switch (dim.kind) {
      case "line_length": {
        const line = params.lines.find((l) => l.line_id === dim.entity_id);
        if (line) {
          w.push_primitive({
            id: dim.dimension_id,
            type: "p2p_distance",
            p1_id: line.start_vertex_id,
            p2_id: line.end_vertex_id,
            distance: dim.value,
          });
        }
        break;
      }
      case "circle_radius": {
        w.push_primitive({
          id: dim.dimension_id,
          type: "circle_radius",
          c_id: dim.entity_id,
          radius: dim.value,
        });
        break;
      }
      case "line_angle": {
        const line = params.lines.find((l) => l.line_id === dim.entity_id);
        if (line) {
          w.push_primitive({
            id: dim.dimension_id,
            type: "p2p_angle",
            p1_id: line.start_vertex_id,
            p2_id: line.end_vertex_id,
            angle: dim.value,
          });
        }
        break;
      }
      case "angle": {
        if (dim.secondary_entity_id.length > 0) {
          w.push_primitive({
            id: dim.dimension_id,
            type: "l2l_angle_ll",
            l1_id: dim.entity_id,
            l2_id: dim.secondary_entity_id,
            angle: dim.value,
          });
        }
        break;
      }
      case "point_distance": {
        if (dim.secondary_entity_id.length > 0) {
          w.push_primitive({
            id: dim.dimension_id,
            type: "p2p_distance",
            p1_id: dim.entity_id,
            p2_id: dim.secondary_entity_id,
            distance: dim.value,
          });
        }
        break;
      }
      default:
        break;
    }
  }

  return vertexIds.length;
}

// ---------------------------------------------------------------------------
// Virtual primitives + solve helpers
// ---------------------------------------------------------------------------

/**
 * Push the virtual draft line (draftStart → cursor) into the solver.
 * draftStart is fixed; cursor is free — the solver moves it to satisfy
 * speculative constraints.
 */
function pushVirtualDraftLine(
  w: any,
  draftStart: [number, number],
  cursor: [number, number],
): void {
  w.push_primitive({
    id: VIRTUAL_DRAFT_POINT_ID,
    type: "point",
    x: draftStart[0],
    y: draftStart[1],
    fixed: true,
  });
  w.push_primitive({
    id: VIRTUAL_CURSOR_POINT_ID,
    type: "point",
    x: cursor[0],
    y: cursor[1],
    fixed: false,
  });
  w.push_primitive({
    id: VIRTUAL_LINE_ID,
    type: "line",
    p1_id: VIRTUAL_DRAFT_POINT_ID,
    p2_id: VIRTUAL_CURSOR_POINT_ID,
  });
}

/** Push a single speculative constraint. */
function pushSpeculativeConstraint(
  w: any,
  snapType: SpeculativeSnapType,
  targetEntityId: string,
  constraintId: string,
): void {
  switch (snapType) {
    case "horizontal_l":
      w.push_primitive({
        id: constraintId,
        type: "horizontal_l",
        l_id: VIRTUAL_LINE_ID,
        temporary: true,
      });
      break;
    case "vertical_l":
      w.push_primitive({
        id: constraintId,
        type: "vertical_l",
        l_id: VIRTUAL_LINE_ID,
        temporary: true,
      });
      break;
    case "tangent_lc":
      // Hard constraint (not temporary) — tangent needs full enforcement.
      // The cursor is the only free point; draftStart is fixed. Temporary
      // (soft) constraints don't reduce DOF and produce non-tangent results.
      w.push_primitive({
        id: constraintId,
        type: "tangent_lc",
        l_id: VIRTUAL_LINE_ID,
        c_id: targetEntityId,
      });
      break;
    case "perpendicular_ll":
      w.push_primitive({
        id: constraintId,
        type: "perpendicular_ll",
        l1_id: VIRTUAL_LINE_ID,
        l2_id: targetEntityId,
        temporary: true,
      });
      break;
    case "parallel":
      w.push_primitive({
        id: constraintId,
        type: "parallel",
        l1_id: VIRTUAL_LINE_ID,
        l2_id: targetEntityId,
        temporary: true,
      });
      break;
    case "point_on_line_pl":
      w.push_primitive({
        id: constraintId,
        type: "point_on_line_pl",
        p_id: VIRTUAL_CURSOR_POINT_ID,
        l_id: targetEntityId,
        temporary: true,
      });
      break;
  }
}

/**
 * Run the INFERENCE-mode solve and extract the cursor position.
 * Returns the result or null if no solved position could be read.
 */
function runInferenceAndReadCursor(
  w: any,
  sketchPointCount: number,
  cursor: [number, number],
): SpeculativeSolveResult {
  w.set_max_iterations(12);
  w.set_convergence_threshold(1e-3);
  const status = w.solve(1); // LevenbergMarquardt = 1

  const ok = status === 0 /* Success */ || status === 1; /* Converged */

  if (ok) {
    w.apply_solution();

    // The cursor point is the LAST point pushed (after all sketch points
    // + draft start). Find its parameter index in the flat param array.
    const allParams: number[] = w.get_gcs_params();
    // Total points pushed: sketch points + virtual draft + virtual cursor
    const totalPoints = sketchPointCount + 2;
    // Cursor point index in the points array = totalPoints - 1
    const cursorPointIndex = totalPoints - 1;
    const solvedX = allParams[cursorPointIndex * 2] ?? cursor[0];
    const solvedY = allParams[cursorPointIndex * 2 + 1] ?? cursor[1];

    const dx = solvedX - cursor[0];
    const dy = solvedY - cursor[1];
    const distance = Math.sqrt(dx * dx + dy * dy);

    return {
      position: [solvedX, solvedY],
      converged: true,
      distance,
      solverStatus: status,
    };
  }

  return {
    position: [cursor[0], cursor[1]],
    converged: false,
    distance: Infinity,
    solverStatus: status,
  };
}

// ---------------------------------------------------------------------------
// Public API — Single-constraint speculative solve
// ---------------------------------------------------------------------------

/**
 * Run a speculative constraint solve to find a snap position.
 *
 * Builds the sketch geometry + existing constraints in the WASM solver,
 * adds a virtual draft line and ONE speculative constraint,
 * runs a fast INFERENCE solve, and returns the solved cursor position.
 *
 * The WASM solver is fully rebuilt on each call (clear_data + push all).
 * For typical sketch sizes (10–50 parameters) with 12 iterations, this
 * completes in ~0.05–0.1ms — well within the 16ms frame budget even
 * when checking 6+ snap types per frame.
 */
export function speculativeSolve({
  bridge,
  params,
  constraints,
  draftStart,
  cursor,
  snapType,
  targetEntityId,
}: SpeculativeSolveParams): SpeculativeSolveResult | null {
  const w = (bridge as any).wrapper;
  if (!w) return null;

  // 1. Build system with sketch geometry + constraints.
  const pointCount = pushSketchGeometryAndConstraints(w, params, constraints);

  // 2. Push virtual draft line.
  pushVirtualDraftLine(w, draftStart, cursor);

  // 3. Push the single speculative constraint.
  pushSpeculativeConstraint(w, snapType, targetEntityId, SPECULATIVE_CONSTRAINT_ID);

  // 4. Solve and read cursor.
  const result = runInferenceAndReadCursor(w, pointCount, cursor);

  if (!result.converged) {
    // Check if the solver detected a conflict from the speculative
    // constraint — this means the snap is geometrically impossible
    // given existing constraints.
    const conflicting = w.get_gcs_conflicting_constraints() as string[];
    if (conflicting.includes(SPECULATIVE_CONSTRAINT_ID)) {
      return null; // Over-constrained — snap not possible
    }
  }

  return result;
}

// ---------------------------------------------------------------------------
// Public API — Multi-constraint speculative solve
// ---------------------------------------------------------------------------

/**
 * Run a speculative solve with MULTIPLE simultaneous temporary constraints.
 *
 * This enables compound snaps that require two constraints to express:
 *   - Line-line intersection:  point_on_line_pl × 2
 *   - Tangent through a line:  tangent_lc + point_on_line_pl
 *   - Perpendicular through midpoint: perpendicular_ll + point_on_line_pl
 *
 * All speculative constraints are pushed with temporary:true, solved
 * together in a single INFERENCE pass, and the cursor point position
 * is read from the solved parameter array.
 *
 * Constraints are identified by sequential IDs: __spec_multi_0, __spec_multi_1, …
 */
export function speculativeMultiSolve({
  bridge,
  params,
  constraints,
  draftStart,
  cursor,
  snapConstraints,
}: SpeculativeMultiSolveParams): SpeculativeSolveResult | null {
  const w = (bridge as any).wrapper;
  if (!w) return null;

  if (snapConstraints.length === 0) return null;

  // 1. Build system with sketch geometry + constraints.
  const pointCount = pushSketchGeometryAndConstraints(w, params, constraints);

  // 2. Push virtual draft line.
  pushVirtualDraftLine(w, draftStart, cursor);

  // 3. Push all speculative constraints simultaneously.
  const constraintIds: string[] = [];
  for (let i = 0; i < snapConstraints.length; i++) {
    const cid = `__spec_multi_${i}`;
    constraintIds.push(cid);
    pushSpeculativeConstraint(w, snapConstraints[i].snapType, snapConstraints[i].targetEntityId, cid);
  }

  // 4. Solve and read cursor.
  const result = runInferenceAndReadCursor(w, pointCount, cursor);

  if (!result.converged) {
    // Check if any speculative constraint was flagged as conflicting.
    const conflicting = w.get_gcs_conflicting_constraints() as string[];
    for (const cid of constraintIds) {
      if (conflicting.includes(cid)) return null; // Over-constrained
    }
  }

  return result;
}