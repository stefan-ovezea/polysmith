import { useCadCoreStore } from "@/state";

/**
 * Small badge displayed next to the sketch toolbar showing solver diagnostics.
 *
 * States:
 *   No solver data → hidden (returns null)
 *   under-constrained → "DOF: N" in amber
 *   fully-constrained → "DOF: 0" in green (briefly shown, then fades)
 *   over-constrained   → "DOF: -N" in red with conflict count
 */
export function SketchDofBadge() {
  const viewport = useCadCoreStore((state) => state.viewport);

  if (!viewport || viewport.solver_dofs === undefined || viewport.solver_dofs === null) {
    return null;
  }

  const dof = viewport.solver_dofs;
  const conflicting = viewport.solver_conflicting_count ?? 0;
  const redundant = viewport.solver_redundant_count ?? 0;

  // No constraints → solver hasn't run, nothing to show.
  if (dof < 0 && conflicting < 0) {
    return null;
  }

  const hasConflicts = conflicting > 0;
  const hasRedundant = redundant > 0;
  const isFullyConstrained = dof === 0 && !hasConflicts;

  let bg: string;
  let fg: string;
  let label: string;

  if (hasConflicts) {
    // Over-constrained: red background
    bg = "#dc2626";
    fg = "#ffffff";
    label = `⚠ ${conflicting}`;
  } else if (isFullyConstrained) {
    // Fully constrained: green
    bg = "#16a34a";
    fg = "#ffffff";
    label = "✓";
  } else if (dof > 0) {
    // Under-constrained: amber
    bg = "#d97706";
    fg = "#ffffff";
    label = `DOF: ${dof}`;
  } else {
    // Redundant only (not conflicting): orange
    bg = "#ea580c";
    fg = "#ffffff";
    label = `~${redundant}`;
  }

  return (
    <span
      className="inline-flex items-center gap-1 rounded px-2 py-0.5 text-xs font-semibold select-none"
      style={{
        backgroundColor: bg,
        color: fg,
        lineHeight: "1.25rem",
      }}
      title={
        hasConflicts
          ? `${conflicting} conflicting constraint${conflicting !== 1 ? "s" : ""}. Remove or edit to resolve.`
          : hasRedundant
            ? `${redundant} redundant constraint${redundant !== 1 ? "s" : ""}.`
            : isFullyConstrained
              ? "Fully constrained"
              : `${dof} degree${dof !== 1 ? "s" : ""} of freedom remaining`
      }
    >
      {label}
    </span>
  );
}
