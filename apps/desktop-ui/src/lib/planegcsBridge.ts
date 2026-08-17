/**
 * planegcsBridge.ts — Polysmith ↔ planegcs WASM constraint solver bridge.
 *
 * Converts Polysmith sketch data (lines, circles, points, constraints,
 * dimensions) into planegcs primitives, runs the solver, and writes
 * updated coordinates back.
 *
 * Two solver modes:
 *   LOOSE  — fast preview during drag (20 iterations, 1e-4 tolerance)
 *   EXACT  — authoritative solve on commit (200 iterations, 1e-10 tolerance)
 *
 * Usage:
 *   const bridge = new PlanegcsBridge(LOOSE);
 *   await bridge.init();
 *   const result = bridge.solve(sketchParams, constraints);
 *   bridge.applyToParams(sketchParams, result);
 */

import {
  make_gcs_wrapper,
  GcsWrapper,
  Algorithm,
} from "@salusoft89/planegcs";
import type {
  SketchFeatureParameters,
  SketchLineEntry,
  SketchCircleEntry,
} from "@/types/geometry/sketch";

// ---------------------------------------------------------------------------
// Constraint data — mirrors C++ SketchConstraint (not yet on TS type)
// ---------------------------------------------------------------------------

export interface SketchConstraintData {
  constraint_id: string;
  kind: string; // "coincident" | "concentric" | "distance" | "angle" | …
  target_ids: string[];
  value?: number;
}

// ---------------------------------------------------------------------------
// Solver configuration
// ---------------------------------------------------------------------------

export interface SolverConfig {
  maxIterations: number;
  convergenceThreshold: number;
  algorithm: Algorithm;
}

export const LOOSE: SolverConfig = {
  maxIterations: 20,
  convergenceThreshold: 1e-4,
  algorithm: Algorithm.LevenbergMarquardt as Algorithm,
};

export const EXACT: SolverConfig = {
  maxIterations: 200,
  convergenceThreshold: 1e-10,
  algorithm: Algorithm.DogLeg as Algorithm,
};

/** Fast, low-accuracy config for speculative snap inferencing.
 *  5 iterations at 1e-3 tolerance — just enough to determine if a
 *  constraint converges and approximately where. Used by the speculative
 *  solve path in pointer-move handlers; the committed position is always
 *  recomputed by the core's EXACT solver on mouse-up. */
export const INFERENCE: SolverConfig = {
  maxIterations: 5,
  convergenceThreshold: 1e-3,
  algorithm: Algorithm.LevenbergMarquardt as Algorithm,
};

/** Snap types that can be resolved via speculative WASM constraints. */
export type SpeculativeSnapType =
  | "tangent_lc"
  | "perpendicular_ll"
  | "parallel"
  | "point_on_line_pl"
  | "horizontal_l"
  | "vertical_l";

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

export interface SolveOutput {
  /** 0=Success, 1=Converged, 2=Failed, 3=SuccessfulSolutionInvalid */
  status: number;
  /** True when the solve converged or succeeded (status 0 or 1). */
  ok: boolean;
  /** Solved point coordinates (id → {x, y}) */
  points: Array<{ id: string; x: number; y: number }>;
  /** Constraint ids that conflict */
  conflicting: string[];
  /** Constraint ids that are redundant */
  redundant: string[];
}

// ---------------------------------------------------------------------------
// Bridge
// ---------------------------------------------------------------------------

export class PlanegcsBridge {
  private wrapper: GcsWrapper | null = null;
  private _config: SolverConfig;
  /** Point ids in the order they were pushed (index → id). */
  private _pointIds: string[] = [];
  /** Number of successful solves since init. */
  solveCount = 0;
  /** Fires once on the first successful solve. Set externally. */
  onFirstSolve: (() => void) | null = null;

  constructor(config: SolverConfig = LOOSE) {
    this._config = config;
  }

  get config(): SolverConfig {
    return this._config;
  }
  set config(c: SolverConfig) {
    this._config = c;
  }

  /** True after init() has succeeded. */
  get isReady(): boolean {
    return this.wrapper !== null;
  }

  /**
   * Initialise the WASM module. Call once at startup.
   * @param wasmPath Optional override path to planegcs.wasm.
   */
  async init(wasmPath?: string): Promise<void> {
    this.wrapper = await make_gcs_wrapper(wasmPath);
  }

  /**
   * Build the planegcs system from Polysmith sketch data, solve, and return
   * updated point coordinates.
   *
   * @param params       Sketch feature parameters (lines, circles, points,
   *                     dimensions, line_relations, midpoint_anchors,
   *                     point_line_anchors).
   * @param constraints  Sketch constraints (coincident, concentric, …).
   *                     Not yet on the TS SketchFeatureParameters type — pass
   *                     from viewport_state.sketch_constraints or the raw IPC
   *                     payload.
   * @param opts.activePointIds  When set, only these point IDs are unfrozen
   *                     (fixed=false); all other points are locked (fixed=true)
   *                     regardless of their stored is_fixed value. Used for
   *                     drag ripple-freeze: only the dragged point and its
   *                     1-hop connections participate in the solve.
   */
  solve(
    params: SketchFeatureParameters,
    constraints: SketchConstraintData[] = [],
    opts?: { activePointIds?: string[] },
  ): SolveOutput {
    if (!this.wrapper) throw new Error("PlanegcsBridge not initialised");

    const w = this.wrapper;
    w.clear_data();
    this._pointIds = [];

    const activeSet = opts?.activePointIds
      ? new Set(opts.activePointIds)
      : null;

    // ---- 1. Push points ------------------------------------------------
    for (const pt of params.vertices) {
      // When activeSet is provided, freeze all points NOT in the active set.
      // Otherwise use the stored is_fixed value.
      const vtxId = pt.vertex_id;
      const frozen = activeSet ? !activeSet.has(vtxId) : pt.is_fixed;
      w.push_primitive({
        id: vtxId,
        type: "point",
        x: pt.x,
        y: pt.y,
        fixed: frozen,
      });
      this._pointIds.push(vtxId);
    }

    // ---- 2. Push lines -------------------------------------------------
    for (const line of params.lines) {
      w.push_primitive({
        id: line.line_id,
        type: "line",
        p1_id: line.start_vertex_id,
        p2_id: line.end_vertex_id,
      });
    }

    // ---- 3. Push circles -----------------------------------------------
    for (const circle of params.circles) {
      const centerId = circle.center_vertex_id ?? `point-circle-${circle.circle_id}-center`;
      w.push_primitive({
        id: circle.circle_id,
        type: "circle",
        c_id: centerId,
        radius: circle.radius,
      });
    }

    // ---- 3b. Push arcs --------------------------------------------------
    for (const arc of params.arcs ?? []) {
      const centerId = arc.center_vertex_id ?? `point-arc-${arc.arc_id}-center`;
      const startAngle = Math.atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
      const endAngle = Math.atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
      w.push_primitive({
        id: arc.arc_id,
        type: "arc",
        c_id: centerId,
        radius: arc.radius,
        start_id: arc.start_vertex_id,
        end_id: arc.end_vertex_id,
        start_angle: startAngle,
        end_angle: endAngle,
      } as any);
    }

    // ---- 4. Constraints ------------------------------------------------

    // 4a. Inline H/V on lines
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

    // 4a2. Arc rules — required constraint for each arc
    for (const arc of params.arcs ?? []) {
      w.push_primitive({
        id: `c-arc-rules-${arc.arc_id}`,
        type: "arc_rules",
        a_id: arc.arc_id,
      } as any);
    }

    // 4b. Coincident / concentric constraints
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

    // 4c. Line relations (parallel, perpendicular, equal_length, tangent_line_circle)
    for (const rel of params.line_relations) {
      const id = rel.relation_id;
      const kind = rel.kind as string; // TS type is narrow; core may send broader
      switch (kind) {
        case "parallel":
          w.push_primitive({
            id,
            type: "parallel",
            l1_id: rel.first_line_id,
            l2_id: rel.second_line_id,
          });
          break;
        case "perpendicular":
          w.push_primitive({
            id,
            type: "perpendicular_ll",
            l1_id: rel.first_line_id,
            l2_id: rel.second_line_id,
          });
          break;
        case "equal_length":
          w.push_primitive({
            id,
            type: "equal_length",
            l1_id: rel.first_line_id,
            l2_id: rel.second_line_id,
          });
          break;
        case "tangent_line_circle":
          // second_line_id is actually the circle id for this relation kind
          w.push_primitive({
            id,
            type: "tangent_lc",
            l_id: rel.first_line_id,
            c_id: rel.second_line_id,
          } as any);
          break;
        default:
          break;
      }
    }

    // 4d. Midpoint anchors → point-on-line
    for (const a of params.midpoint_anchors) {
      w.push_primitive({
        id: a.anchor_id,
        type: "point_on_line_pl",
        p_id: a.vertex_id,
        l_id: a.line_id,
      });
    }

    // 4e. Point-line anchors → point-on-line
    for (const a of params.point_line_anchors) {
      w.push_primitive({
        id: a.anchor_id,
        type: "point_on_line_pl",
        p_id: a.vertex_id,
        l_id: a.line_id,
      });
    }

    // 4f. Dimensional constraints (dimensions with user-set expressions)
    for (const dim of params.dimensions) {
      if (dim.driven) continue;
      // Skip auto-dimensions that are display-only (no user expression).
      // The TS type doesn't carry is_auto; we use empty expression as a
      // proxy — manual dims always have a value.
      if (dim.expression.length === 0) continue;

      switch (dim.kind) {
        case "line_length": {
          const line = findLine(params.lines, dim.entity_id);
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
        case "line_angle": {
          const line = findLine(params.lines, dim.entity_id);
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
        case "circle_radius": {
          w.push_primitive({
            id: dim.dimension_id,
            type: "circle_radius",
            c_id: dim.entity_id,
            radius: dim.value,
          });
          break;
        }
        case "angle": {
          // Angle between two lines (entity_id + secondary_entity_id)
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
        case "circle_center_distance": {
          if (dim.secondary_entity_id.length > 0) {
            const cid1 = `point-circle-${dim.entity_id}-center`;
            const cid2 = `point-circle-${dim.secondary_entity_id}-center`;
            w.push_primitive({
              id: dim.dimension_id,
              type: "p2p_distance",
              p1_id: cid1,
              p2_id: cid2,
              distance: dim.value,
            });
          }
          break;
        }
        case "circle_line_distance": {
          if (dim.secondary_entity_id.length > 0) {
            w.push_primitive({
              id: dim.dimension_id,
              type: "c2ldistance",
              c_id: dim.entity_id,
              l_id: dim.secondary_entity_id,
              dist: dim.value,
            } as any);
          }
          break;
        }
        default:
          break;
      }
    }

    // ---- 5. Configure and solve ---------------------------------------
    w.set_max_iterations(this._config.maxIterations);
    w.set_convergence_threshold(this._config.convergenceThreshold);
    const status = w.solve(this._config.algorithm);

    // ---- 6. Read results ----------------------------------------------
    const ok = status === 0 /* Success */ || status === 1; /* Converged */

    const result: SolveOutput = {
      status,
      ok,
      points: [],
      conflicting: [],
      redundant: [],
    };

    if (ok) {
      w.apply_solution();

      // Read solved point coordinates from the flat parameter array.
      // Order: each point → x, y (2 params), then circle radii.
      const allParams = w.get_gcs_params();
      for (let i = 0; i < this._pointIds.length; i++) {
        result.points.push({
          id: this._pointIds[i],
          x: allParams[i * 2] ?? 0,
          y: allParams[i * 2 + 1] ?? 0,
        });
      }

      result.conflicting = w.get_gcs_conflicting_constraints();
      result.redundant = w.get_gcs_redundant_constraints();

      this.solveCount++;
      if (this.solveCount === 1 && this.onFirstSolve) {
        this.onFirstSolve();
      }
    }

    return result;
  }

  /**
   * Write solved point coordinates back into the sketch parameters'
   * canonical storage (lines, circles). Updates line start_x/start_y,
   * end_x/end_y and circle center_x/center_y in place.
   */
  applyToParams(
    params: SketchFeatureParameters,
    output: SolveOutput,
  ): void {
    for (const line of params.lines) {
      const sp = output.points.find((p) => p.id === line.start_vertex_id);
      const ep = output.points.find((p) => p.id === line.end_vertex_id);
      if (sp) {
        line.start_x = sp.x;
        line.start_y = sp.y;
      }
      if (ep) {
        line.end_x = ep.x;
        line.end_y = ep.y;
      }
    }

    for (const circle of params.circles) {
      const centerId = circle.center_vertex_id ?? `point-circle-${circle.circle_id}-center`;
      const cp = output.points.find((p) => p.id === centerId);
      if (cp) {
        circle.center_x = cp.x;
        circle.center_y = cp.y;
      }
      // Circle radius updates from solver not yet written back
      // (requires mapping the radius parameter index).
    }
  }

  /** Release the WASM module. */
  destroy(): void {
    if (this.wrapper) {
      this.wrapper.destroy_gcs_module();
      this.wrapper = null;
    }
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function findLine(
  lines: SketchLineEntry[],
  lineId: string,
): SketchLineEntry | undefined {
  return lines.find((l) => l.line_id === lineId);
}
