type PointerHoverHit =
  | {
      kind: string;
      id?: string | null;
    }
  | null
  | undefined;

interface HoverSetters {
  setHoveredReference: (id: string | null) => void;
  setHoveredPrimitive: (id: string | null) => void;
  setHoveredFace: (id: string | null) => void;
  setHoveredEdge: (id: string | null) => void;
  setHoveredVertex: (id: string | null) => void;
  setHoveredSketchProfile: (id: string | null) => void;
  setHoveredSketchPoint: (id: string | null) => void;
  setHoveredSketchEntity: (id: string | null) => void;
}

interface DraftFeedbackClearers {
  clearPreviewLine: () => void;
  clearPreviewCircle: () => void;
  clearPreviewArc: () => void;
  clearPreviewDimension: () => void;
  setSketchSnapLabel: (label: string | null) => void;
  setConstraintPreview: (preview: null) => void;
  clearDraftDimensionSession: () => void;
}

export interface PointerMoveHoverActions
  extends HoverSetters,
    DraftFeedbackClearers {}

function hitId(hit: PointerHoverHit, kind: string) {
  return hit?.kind === kind ? hit.id ?? null : null;
}

export function clearSketchDraftFeedback(actions: DraftFeedbackClearers) {
  actions.clearPreviewLine();
  actions.clearPreviewCircle();
  actions.clearPreviewArc();
  actions.clearPreviewDimension();
  actions.setSketchSnapLabel(null);
  actions.setConstraintPreview(null);
  actions.clearDraftDimensionSession();
}

export function clearSketchEntityHover(actions: HoverSetters) {
  actions.setHoveredSketchProfile(null);
  actions.setHoveredSketchPoint(null);
  actions.setHoveredSketchEntity(null);
}

function clearBodyHover(actions: HoverSetters) {
  actions.setHoveredFace(null);
  actions.setHoveredEdge(null);
  actions.setHoveredVertex(null);
}

function applyBodyHover(hit: PointerHoverHit, actions: HoverSetters) {
  actions.setHoveredFace(hitId(hit, "face"));
  actions.setHoveredEdge(hitId(hit, "edge"));
  actions.setHoveredVertex(hitId(hit, "vertex"));
}

export function applySelectToolHover(
  hit: PointerHoverHit,
  actions: PointerMoveHoverActions,
) {
  clearSketchDraftFeedback(actions);
  actions.setHoveredReference(null);
  actions.setHoveredPrimitive(null);
  actions.setHoveredFace(null);
  actions.setHoveredEdge(null);
  actions.setHoveredVertex(null);
  actions.setHoveredSketchPoint(hitId(hit, "sketch_point"));
  actions.setHoveredSketchEntity(hitId(hit, "sketch_entity"));
  actions.setHoveredSketchProfile(hitId(hit, "sketch_profile"));
}

export function applyProjectToolHover(
  hit: PointerHoverHit,
  actions: PointerMoveHoverActions,
) {
  clearSketchDraftFeedback(actions);
  actions.setHoveredReference(null);
  actions.setHoveredPrimitive(null);
  actions.setHoveredSketchProfile(null);
  actions.setHoveredSketchPoint(null);
  actions.setHoveredSketchEntity(null);
  applyBodyHover(hit, actions);
}

export function applyTrimToolHover(
  hit: PointerHoverHit,
  actions: PointerMoveHoverActions,
) {
  clearSketchDraftFeedback(actions);
  actions.setHoveredSketchEntity(hitId(hit, "sketch_entity"));
  actions.setHoveredSketchProfile(null);
  actions.setHoveredSketchPoint(null);
  clearBodyHover(actions);
}

export function applySceneHover(
  hit: PointerHoverHit,
  actions: HoverSetters,
) {
  if (hit?.kind === "sketch_dimension" || hit?.kind === "sketch_entity") {
    actions.setHoveredReference(null);
    actions.setHoveredPrimitive(null);
    actions.setHoveredSketchProfile(null);
    actions.setHoveredSketchPoint(null);
    actions.setHoveredSketchEntity(hitId(hit, "sketch_entity"));
    clearBodyHover(actions);
    return;
  }

  actions.setHoveredReference(hitId(hit, "reference"));
  actions.setHoveredSketchProfile(hitId(hit, "sketch_profile"));
  actions.setHoveredSketchPoint(hitId(hit, "sketch_point"));
  actions.setHoveredSketchEntity(null);
  applyBodyHover(hit, actions);
  actions.setHoveredPrimitive(hitId(hit, "primitive"));
}
