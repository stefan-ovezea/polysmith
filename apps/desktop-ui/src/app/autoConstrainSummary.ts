// What did "Fully constrain" actually do?
//
// The command reply is a document state and a log line — the counts live only in
// the Logs panel, which is not where a user looks after clicking a button. The UI
// therefore measures the run itself from the state before and after, which needs
// no protocol change (an undeclared reply field would be dropped by the zod
// schema and surface as a command timeout).

export type AutoConstrainSummary = {
  dofsBefore: number | null;
  dofsAfter: number | null;
  dimensions: number;
  pins: number;
  constraints: number;
};

type SketchSnapshot = {
  dofs: number | null;
  dimensions: number;
  pins: number;
  /** Constraint records plus line relations: both pin geometry. */
  constraints: number;
};

/**
 * Reads the numbers the summary needs out of one sketch's parameters.
 *
 * Only driving dimensions are counted: an automatic one is a measurement, not
 * something the pass added on purpose.
 */
export function readSketchSnapshot(feature: {
  sketch_parameters?: {
    solver_dofs?: number;
    dimensions?: Array<{ driven?: boolean; is_auto?: boolean; expression?: string }>;
    vertices?: Array<{ is_fixed?: boolean }>;
    constraints?: unknown[];
    line_relations?: unknown[];
  } | null;
} | null): SketchSnapshot {
  const parameters = feature?.sketch_parameters ?? null;
  if (parameters === null) {
    return { dofs: null, dimensions: 0, pins: 0, constraints: 0 };
  }
  const drivingDimensions = (parameters.dimensions ?? []).filter(
    (dimension) =>
      !dimension.driven &&
      !((dimension.is_auto ?? false) && (dimension.expression ?? "") === ""),
  ).length;
  const pins = (parameters.vertices ?? []).filter(
    (vertex) => vertex.is_fixed === true,
  ).length;
  const dofs =
    typeof parameters.solver_dofs === "number" && parameters.solver_dofs >= 0
      ? parameters.solver_dofs
      : null;
  return {
    dofs,
    dimensions: drivingDimensions,
    pins,
    constraints:
      (parameters.constraints?.length ?? 0) +
      (parameters.line_relations?.length ?? 0),
  };
}

/** Deltas between the sketch before and after the run. */
export function summarizeAutoConstrainRun(
  before: SketchSnapshot,
  after: SketchSnapshot,
): AutoConstrainSummary {
  return {
    dofsBefore: before.dofs,
    dofsAfter: after.dofs,
    dimensions: Math.max(0, after.dimensions - before.dimensions),
    pins: Math.max(0, after.pins - before.pins),
    constraints: Math.max(0, after.constraints - before.constraints),
  };
}

/** True when the run changed nothing, so the UI should not claim it did. */
export function autoConstrainSummaryIsEmpty(
  summary: AutoConstrainSummary,
): boolean {
  return (
    summary.dimensions === 0 && summary.pins === 0 && summary.constraints === 0
  );
}
