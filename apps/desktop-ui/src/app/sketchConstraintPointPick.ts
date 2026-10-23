import type { ArmedSketchConstraint } from "../types";

export type SketchConstraintVertexKind = "endpoint" | "center" | "quadrant";

export interface SketchConstraintVertexPickContext {
  vertexId: string;
  kind: SketchConstraintVertexKind;
  additive: boolean;
  armedSketchConstraint: ArmedSketchConstraint;
  selectSketchPoint: (vertexId: string, additive?: boolean) => Promise<void>;
  setSketchCoincidentConstraint: (
    vertexId: string,
    otherVertexId: string,
  ) => Promise<void>;
  setSketchPointFixed: (vertexId: string, isFixed: boolean) => Promise<void>;
  setArmedSketchConstraint: (constraint: ArmedSketchConstraint) => void;
  addMessage: (message: string) => void;
}

export async function handleSketchConstraintVertexPickFromContext(
  context: SketchConstraintVertexPickContext,
) {
  const { armedSketchConstraint } = context;

  // Fix constraint: arm, click a point → fix it, then clear the arm
  // so the next click doesn't accidentally fix another point.
  if (armedSketchConstraint?.kind === "fixed") {
    await context.setSketchPointFixed(context.vertexId, true);
    context.setArmedSketchConstraint(null);
    return;
  }

  // Clear constraint: arm, click a point → unfix it.
  if (armedSketchConstraint?.kind === "clear") {
    await context.setSketchPointFixed(context.vertexId, false);
    context.setArmedSketchConstraint(null);
    return;
  }

  // Midpoint (S1): pick the POINT first, then the line it should sit on. The
  // line pick completes the constraint (see sketchConstraintLinePick.ts).
  if (armedSketchConstraint?.kind === "midpoint") {
    await context.selectSketchPoint(context.vertexId);
    context.setArmedSketchConstraint({
      kind: "midpoint",
      firstPointId: context.vertexId,
    });
    return;
  }

  // Symmetric (S1): point, point, axis line. The line pick applies it.
  if (armedSketchConstraint?.kind === "symmetric") {
    if (!armedSketchConstraint.firstPointId) {
      await context.selectSketchPoint(context.vertexId);
      context.setArmedSketchConstraint({
        kind: "symmetric",
        firstPointId: context.vertexId,
        secondPointId: null,
      });
      return;
    }
    if (armedSketchConstraint.firstPointId === context.vertexId) {
      return;
    }
    if (!armedSketchConstraint.secondPointId) {
      await context.selectSketchPoint(context.vertexId, true);
      context.setArmedSketchConstraint({
        kind: "symmetric",
        firstPointId: armedSketchConstraint.firstPointId,
        secondPointId: context.vertexId,
      });
      return;
    }
    // Both points are already armed — the next click must be the axis line.
    await context.selectSketchPoint(context.vertexId, true);
    return;
  }

  if (!armedSketchConstraint || armedSketchConstraint.kind !== "coincident") {
    await context.selectSketchPoint(context.vertexId, context.additive);
    return;
  }

  if (!armedSketchConstraint.firstPointId) {
    context.addMessage(
      `coincident: first point ${context.vertexId} (${context.kind})`,
    );
    await context.selectSketchPoint(context.vertexId);
    context.setArmedSketchConstraint({
      kind: "coincident",
      firstPointId: context.vertexId,
    });
    return;
  }

  if (armedSketchConstraint.firstPointId === context.vertexId) {
    context.addMessage("coincident: same point clicked twice, ignoring");
    return;
  }

  context.addMessage(
    `coincident: second point ${context.vertexId} (${context.kind}) - applying constraint`,
  );
  await context.setSketchCoincidentConstraint(
    context.vertexId,
    armedSketchConstraint.firstPointId,
  );
  context.addMessage("coincident: constraint applied, armed for next pair");
  context.setArmedSketchConstraint({
    kind: "coincident",
    firstPointId: null,
  });
}
