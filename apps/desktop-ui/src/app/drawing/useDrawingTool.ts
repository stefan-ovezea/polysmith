// The R1 drawing tool state machine — the UI-side "which drawing tool
// is armed" state (Core-UI principle: interaction state lives in the
// UI; the core knows nothing about the armed tool).  The hook owns the
// tool, its settings, the projected parent, and the cursor-sector
// derivation with the exact-slot offset learning; the App keeps
// ownership of IPC round-trips (the callbacks) and the panels.

import { useCallback, useEffect, useMemo, useRef, useState } from "react";

import { useCadCoreStore } from "@/state";
import type {
  Drawing,
  DrawingView,
  DrawingViewFrame,
  DrawingViewPreviewPayload,
  ViewportDrawingSheet,
} from "@/types";

import {
  buildIsoFrame,
  buildProjectedChild,
  clampSheetPosition,
  classifySector,
  customFrameBasis,
  resolveBodyIds,
  SECTOR_DIRS,
  standardViewFrame,
  viewBasisOf,
  VIEW_GAP_MM,
} from "@/lib/drawingViewMath";
import type {
  BaseOrientation,
  BaseViewSettings,
  DrawingTool,
  GhostFrame,
  ProjectedParent,
  SectorInfo,
} from "@/types";

export interface UseDrawingToolInputs {
  activeDrawing: Drawing | undefined;
  /** viewport.drawing_sheets[0] — parent bounds + origins source. */
  activeSheet: ViewportDrawingSheet | undefined;
  sheetSize: { width_mm: number; height_mm: number };
  projectionAngle: "first_angle" | "third_angle";
  availableBodies: Array<{
    id: string;
    label: string;
    center: { x: number; y: number; z: number };
  }>;
  /** The selected-or-all body ids App derives (the old auto-view). */
  defaultBodyIds: string[];
  /** The base ghost's resting position when the cursor leaves the
   *  sheet (App's nextDrawingSheetPosition). */
  defaultGhostPosition: [number, number];
  /** The placement cursor (shared with the section panel). */
  cursorPoint: [number, number] | null;
  /** The latest drawing_view_preview reply (offset + size learning). */
  previewReply: DrawingViewPreviewPayload | null;
  callbacks: {
    onCreateBase: (def: DrawingView) => Promise<void>;
    onCreateProjected: (def: DrawingView, parentId: string) => Promise<void>;
    onDeleteView?: (viewId: string) => Promise<void>;
  };
}

/** A learned content placement: the orientation-dependent origin→min
 *  offset and the content size, taken from a preview reply. */
interface LearnedPlacement {
  offset: [number, number];
  size: [number, number];
  /** true once the position was corrected with this offset — further
   *  re-sends must not loop. */
  corrected: boolean;
}

interface PendingDefinition {
  def: DrawingView;
  /** The sector this def belongs to (null for the base ghost). */
  sector: SectorInfo | null;
  /** The offset-cache key + estimate used to build it — the reply
   *  validator for the learning effect. */
  key: string;
  estimate: [number, number];
  /** The core's label for this orientation — the reply-match key for
   *  the content curves (the payload echoes `view.label`). */
  label: string;
  /** The locally derived placement frame (cursor + learned size) —
   *  null until a content size is known (no preview reply yet). */
  ghost: GhostFrame | null;
}

export interface DrawingToolApi {
  tool: DrawingTool;
  base: BaseViewSettings;
  projectedParent: ProjectedParent | null;
  selectedViewId: string | null;
  /** The uncommitted definition the App debounces into
   *  drawing_view_preview (null = no ghost). */
  previewDefinition: DrawingView | null;
  /** true while the placement cache lacks this orientation/sector —
   *  the App sends ONE preview per orientation (a position-only move
   *  never re-sends; the local ghost frame + content translation
   *  track the cursor). */
  previewNeeded: boolean;
  /** The locally derived ghost placement frame (see GhostFrame) —
   *  drawn at cursor speed with no core round-trip. */
  ghostFrame: GhostFrame | null;
  /** The live sector in projected mode (null = dead zone / no ghost). */
  activeSector: SectorInfo | null;
  armBaseView: () => void;
  armProjectedView: () => void;
  /** Arms the projected tool bound to a caller-composed parent (the
   *  App composes it from fresh store state after a base commit). */
  armProjectedFrom: (parent: ProjectedParent) => void;
  armSection: () => void;
  armMove: () => void;
  armDeleteView: () => void;
  cancel: () => void;
  setOrientation: (orientation: BaseOrientation) => void;
  setCurrent3dFrame: (frame: DrawingViewFrame | null) => void;
  setScale: (scale: number) => void;
  setShowHidden: (showHidden: boolean) => void;
  setBodyChoice: (bodyChoice: string) => void;
  /** Sets the projected parent (composed by the App — it reads the
   *  fresh store state after a create, before the props re-render). */
  pickProjectedParent: (parent: ProjectedParent) => void;
  /** Commits the armed tool at a sheet click (dispatch by tool). */
  handleCommit: (point: [number, number]) => void;
  selectViewForDelete: (viewId: string) => void;
  deleteSelectedView: () => Promise<void>;
  reset: () => void;
}

const DEAD_ZONE_MM = 10;
const OFFSET_TOLERANCE_MM = 0.5;

/** The projected parent's own origin→min offset — the seed estimate
 *  for a child's slot correction. */
function parentOffsetEstimate(parent: ProjectedParent): [number, number] {
  return [
    parent.bounds.origin[0] - parent.bounds.min[0],
    parent.bounds.origin[1] - parent.bounds.min[1],
  ];
}

/** The core's view label for a definition — the preview payload
 *  echoes it, so it is the content-matching key between the local
 *  ghost and the last reply's curves. */
function defLabel(def: DrawingView): string {
  return def.standard_view && def.standard_view !== ""
    ? def.standard_view
    : "axonometric";
}

/** The content size a preview reply recorded for a label+scale — the
 *  local ghost frame's size before the learning effect has written
 *  the placement-cache entry. */
function sizeFromReply(
  reply: DrawingViewPreviewPayload | null,
  label: string,
  scale: number,
): [number, number] | null {
  if (!reply || reply.view.label !== label || reply.view.scale !== scale) {
    return null;
  }
  const width = reply.view.max[0] - reply.view.min[0];
  const height = reply.view.max[1] - reply.view.min[1];
  if (width <= 0 || height <= 0) {
    return null;
  }
  return [width, height];
}

export function useDrawingTool(inputs: UseDrawingToolInputs): DrawingToolApi {
  const {
    activeDrawing,
    activeSheet,
    sheetSize,
    projectionAngle,
    availableBodies,
    defaultBodyIds,
    defaultGhostPosition,
    cursorPoint,
    previewReply,
    callbacks,
  } = inputs;

  const [tool, setTool] = useState<DrawingTool>("idle");
  const [base, setBase] = useState<BaseViewSettings>({
    orientation: "front",
    customFrame: null,
    scale: 1,
    showHidden: false,
    bodyChoice: "__all__",
  });
  // The resolved basis of the current base orientation — the iso
  // buttons derive from it (NE from "front" ≠ NE from "top").
  const [baseBasis, setBaseBasis] = useState(() =>
    standardViewFrame("front"),
  );
  const [projectedParent, setProjectedParent] =
    useState<ProjectedParent | null>(null);
  const [selectedViewId, setSelectedViewId] = useState<string | null>(null);
  // Bumped when a learned offset corrects a slot — forces one re-send.
  const [correctionTick, setCorrectionTick] = useState(0);
  const placementCacheRef = useRef<Map<string, LearnedPlacement>>(new Map());
  const callbacksRef = useRef(callbacks);
  callbacksRef.current = callbacks;

  // ── Tool arming / cancelling ─────────────────────────────────────

  const cancel = useCallback(() => {
    setTool("idle");
    setProjectedParent(null);
    setSelectedViewId(null);
    // A cancelled placement session re-learns its placements from
    // scratch on the next arm (also drops the last preview's content
    // so a re-armed orientation always re-requests its curves).
    placementCacheRef.current.clear();
  }, []);

  const armBaseView = useCallback(() => {
    setTool("base_view");
    setProjectedParent(null);
    setSelectedViewId(null);
    setBase((previous) => ({
      ...previous,
      bodyChoice:
        defaultBodyIds.length === 1 ? defaultBodyIds[0] : "__all__",
    }));
  }, [defaultBodyIds]);

  const armProjectedView = useCallback(() => {
    // Auto-parent the first committed view (the base view in the
    // normal flow) when there is one — else the frame click picks.
    const view = activeDrawing?.views[0];
    const bounds = view
      ? activeSheet?.views.find((entry) => entry.view_id === view.view_id)
      : undefined;
    const basis = view ? viewBasisOf(view) : null;
    if (view && bounds && basis) {
      setProjectedParent({
        viewId: view.view_id,
        bounds: {
          min: bounds.min,
          max: bounds.max,
          origin: bounds.origin,
        },
        basis,
        scale: view.scale,
        showHidden: view.show_hidden,
        sourceBodyIds: view.source_body_ids,
      });
    } else {
      setProjectedParent(null);
    }
    setTool("projected_view");
    setSelectedViewId(null);
  }, [activeDrawing, activeSheet]);

  const armProjectedFrom = useCallback((parent: ProjectedParent) => {
    setProjectedParent(parent);
    setTool("projected_view");
    setSelectedViewId(null);
  }, []);

  const armSection = useCallback(() => {
    setTool("section");
    setProjectedParent(null);
    setSelectedViewId(null);
  }, []);

  const armMove = useCallback(() => {
    setTool("move");
    setProjectedParent(null);
    setSelectedViewId(null);
  }, []);

  const armDeleteView = useCallback(() => {
    setTool("delete_view");
    setProjectedParent(null);
    setSelectedViewId(null);
  }, []);

  // ── Settings ─────────────────────────────────────────────────────

  const setOrientation = useCallback(
    (orientation: BaseOrientation) => {
      setBase((previous) => {
        // Resolve the NEW basis from the OLD one for iso choices —
        // NE of a top-oriented base differs from NE of the front.
        let nextBasis = baseBasis;
        if (
          orientation === "front" ||
          orientation === "right" ||
          orientation === "left" ||
          orientation === "top" ||
          orientation === "bottom" ||
          orientation === "back"
        ) {
          nextBasis = standardViewFrame(orientation);
        } else if (
          orientation === "ne" ||
          orientation === "nw" ||
          orientation === "se" ||
          orientation === "sw"
        ) {
          const frame = buildIsoFrame(baseBasis, orientation);
          if (frame) {
            nextBasis = customFrameBasis(frame);
          }
        } else if (previous.customFrame) {
          nextBasis = customFrameBasis(previous.customFrame);
        }
        setBaseBasis(nextBasis);
        return { ...previous, orientation };
      });
    },
    [baseBasis],
  );

  const setCurrent3dFrame = useCallback((frame: DrawingViewFrame | null) => {
    if (!frame) {
      return;
    }
    setBaseBasis(customFrameBasis(frame));
    setBase((previous) => ({
      ...previous,
      orientation: "current3d",
      customFrame: frame,
    }));
  }, []);

  const setScale = useCallback((scale: number) => {
    setBase((previous) => ({ ...previous, scale }));
  }, []);

  const setShowHidden = useCallback((showHidden: boolean) => {
    setBase((previous) => ({ ...previous, showHidden }));
  }, []);

  const setBodyChoice = useCallback((bodyChoice: string) => {
    setBase((previous) => ({ ...previous, bodyChoice }));
  }, []);

  const pickProjectedParent = useCallback((parent: ProjectedParent) => {
    setProjectedParent(parent);
  }, []);

  const selectViewForDelete = useCallback((viewId: string) => {
    setSelectedViewId(viewId);
  }, []);

  const deleteSelectedView = useCallback(async () => {
    const viewId = selectedViewId;
    if (!viewId || !callbacksRef.current.onDeleteView) {
      return;
    }
    await callbacksRef.current.onDeleteView(viewId);
    setSelectedViewId(null);
  }, [selectedViewId]);

  const reset = useCallback(() => {
    cancel();
  }, [cancel]);

  // The armed tool must not outlive its context: an undone parent
  // view (Ctrl+Z after a base placement) cancels the projected tool —
  // the ghost must not keep deriving children from a phantom parent.
  // The check reads the FRESH store state: right after a base commit
  // the App arms the tool before the props re-render, and the store
  // already contains the new view (the commit awaited the document
  // change first).
  useEffect(() => {
    if (tool !== "projected_view" || !projectedParent) {
      return;
    }
    const drawingState = useCadCoreStore.getState().document?.drawing;
    const freshDrawing = drawingState?.drawings.find(
      (entry) => entry.drawing_id === drawingState.active_drawing_id,
    );
    if (
      freshDrawing &&
      !freshDrawing.views.some((view) => view.view_id === projectedParent.viewId)
    ) {
      cancel();
    }
  }, [tool, projectedParent, cancel]);

  // ── The pending (ghost) definition ───────────────────────────────

  // The base ghost's offset-cache key: the offset is orientation-
  // dependent (position-independent — the flatten is linear), so one
  // learned reply teaches every later position of the same setup.
  const baseKey = useMemo(() => {
    const bodyIds = resolveBodyIds(base.bodyChoice, availableBodies).join(
      "|",
    );
    const frameKey =
      base.orientation === "current3d" && base.customFrame
        ? `${base.orientation}:${base.customFrame.normal
            .map((value) => value.toFixed(6))
            .join(",")}`
        : base.orientation;
    return `base:${frameKey}:${base.scale}:${base.showHidden ? 1 : 0}:${bodyIds}`;
  }, [base, availableBodies]);

  // The uncommitted definition behind the ghost, plus the reply-
  // validation bookkeeping for the offset learning.  For projected
  // mode the sector classification, slot computation and cache
  // correction all live here — a cursor move re-derives everything.
  const pending = useMemo<PendingDefinition | null>(() => {
    if (tool !== "base_view" && tool !== "projected_view") {
      return null;
    }
    if (tool === "base_view") {
      const sourceBodyIds = resolveBodyIds(base.bodyChoice, availableBodies);
      if (sourceBodyIds.length === 0) {
        return null;
      }
      // The ghost follows the cursor EXACTLY (Fusion-like) — the
      // learned origin→min offset keeps the content min on the
      // cursor; only the COMMIT clamps the position onto the sheet.
      const position = cursorPoint ?? defaultGhostPosition;
      const entry = placementCacheRef.current.get(baseKey);
      const estimate = entry?.offset ?? [0, 0];
      const sheetPosition: [number, number] = [
        position[0] - estimate[0],
        position[1] - estimate[1],
      ];

      let def: DrawingView;
      if (
        base.orientation === "front" ||
        base.orientation === "right" ||
        base.orientation === "left" ||
        base.orientation === "top" ||
        base.orientation === "bottom" ||
        base.orientation === "back"
      ) {
        def = {
          view_id: "",
          kind: "projection",
          standard_view: base.orientation,
          source_body_ids: sourceBodyIds,
          scale: base.scale,
          sheet_position: sheetPosition,
          show_hidden: base.showHidden,
          warning: "",
        };
      } else if (
        base.orientation === "ne" ||
        base.orientation === "nw" ||
        base.orientation === "se" ||
        base.orientation === "sw"
      ) {
        const frame = buildIsoFrame(baseBasis, base.orientation);
        if (!frame) {
          return null;
        }
        def = {
          view_id: "",
          kind: "axonometric",
          standard_view: "",
          custom_frame: frame,
          source_body_ids: sourceBodyIds,
          scale: base.scale,
          sheet_position: sheetPosition,
          show_hidden: base.showHidden,
          warning: "",
        };
      } else {
        // "current3d" — the captured camera frame (null until the
        // user captures one; the strip disables the choice without).
        if (!base.customFrame) {
          return null;
        }
        def = {
          view_id: "",
          kind: "axonometric",
          standard_view: "",
          custom_frame: base.customFrame,
          source_body_ids: sourceBodyIds,
          scale: base.scale,
          sheet_position: sheetPosition,
          show_hidden: base.showHidden,
          warning: "",
        };
      }

      // The local ghost frame: content min on the cursor, size from
      // the learned entry — or the matching preview reply's bounds
      // while the first reply is still in flight.
      const label = defLabel(def);
      const size = entry?.size ?? sizeFromReply(previewReply, label, base.scale);
      const ghost: GhostFrame | null =
        size && size[0] > 0 && size[1] > 0
          ? {
              min: [position[0], position[1]],
              max: [position[0] + size[0], position[1] + size[1]],
              label,
              scale: base.scale,
            }
          : null;
      return { def, sector: null, key: baseKey, estimate, label, ghost };
    }

    // projected_view
    const parent = projectedParent;
    if (!parent || !cursorPoint) {
      return null;
    }
    const bounds = parent.bounds;
    // Dead zone: over the parent itself the cursor picks no sector —
    // the ghost hides and a click there does nothing.
    if (
      cursorPoint[0] > bounds.min[0] - DEAD_ZONE_MM &&
      cursorPoint[0] < bounds.max[0] + DEAD_ZONE_MM &&
      cursorPoint[1] > bounds.min[1] - DEAD_ZONE_MM &&
      cursorPoint[1] < bounds.max[1] + DEAD_ZONE_MM
    ) {
      return null;
    }
    const centerX = (bounds.min[0] + bounds.max[0]) / 2;
    const centerY = (bounds.min[1] + bounds.max[1]) / 2;
    const sectorIndex = classifySector(
      cursorPoint[0] - centerX,
      cursorPoint[1] - centerY,
    );
    if (sectorIndex === null) {
      return null;
    }
    // The child inherits scale/bodies, so its content size equals the
    // parent's exactly — the slot clamp needs no estimation.
    const childSize: [number, number] = [
      bounds.max[0] - bounds.min[0],
      bounds.max[1] - bounds.min[1],
    ];
    // Follow-cursor placement (Fusion-like): the child's CENTER rides
    // the sector's alignment line through the parent center; the
    // distance follows the cursor with the slot gap as the floor, so
    // the ghost travels with the mouse (up, down, far to the side)
    // instead of being pinned next to the parent at its level.
    const dir = SECTOR_DIRS[sectorIndex];
    const dirLen = Math.hypot(dir[0], dir[1]);
    const ux = dir[0] / dirLen;
    const uy = dir[1] / dirLen;
    const parentSupport =
      ((bounds.max[0] - bounds.min[0]) / 2) * Math.abs(ux) +
      ((bounds.max[1] - bounds.min[1]) / 2) * Math.abs(uy);
    const childSupport =
      (childSize[0] / 2) * Math.abs(ux) + (childSize[1] / 2) * Math.abs(uy);
    const minDist = parentSupport + VIEW_GAP_MM + childSupport;
    const along =
      (cursorPoint[0] - centerX) * ux + (cursorPoint[1] - centerY) * uy;
    const distance = Math.max(minDist, along);
    const rawMin: [number, number] = [
      centerX + ux * distance - childSize[0] / 2,
      centerY + uy * distance - childSize[1] / 2,
    ];
    // Clamp through the direction-preserving fallback: a child that
    // cannot fit within the margins keeps following the cursor
    // (bleeding off the opposite paper edge) instead of being pinned
    // to the parent's level.
    const clampedMin = clampSheetPosition(
      rawMin,
      childSize[0],
      childSize[1],
      sheetSize.width_mm,
      sheetSize.height_mm,
    );
    const key = projectedKey(parent, sectorIndex, projectionAngle);
    const entry = placementCacheRef.current.get(key);
    const estimate = entry?.offset ?? parentOffsetEstimate(parent);
    const sheetPosition: [number, number] = [
      clampedMin[0] - estimate[0],
      clampedMin[1] - estimate[1],
    ];
    const built = buildProjectedChild(
      parent.basis,
      sectorIndex,
      projectionAngle,
      sheetPosition,
      {
        scale: parent.scale,
        showHidden: parent.showHidden,
        sourceBodyIds: parent.sourceBodyIds,
      },
    );
    if (!built) {
      return null;
    }
    const label = defLabel(built.def);
    const ghost: GhostFrame = {
      min: [clampedMin[0], clampedMin[1]],
      max: [clampedMin[0] + childSize[0], clampedMin[1] + childSize[1]],
      label,
      scale: parent.scale,
    };
    return {
      def: built.def,
      sector: {
        index: sectorIndex,
        isDiagonal: built.isDiagonal,
        sheetDir: SECTOR_DIRS[sectorIndex],
        childDef: built.def,
      },
      key,
      estimate,
      label,
      ghost,
    };
  }, [
    tool,
    base,
    baseBasis,
    baseKey,
    availableBodies,
    cursorPoint,
    defaultGhostPosition,
    sheetSize,
    projectedParent,
    projectionAngle,
    correctionTick,
    previewReply,
  ]);

  // ── Offset learning ──────────────────────────────────────────────

  // The key of the definition the App most recently debounced into
  // drawing_view_preview — the only reply whose learning may teach
  // the CURRENT pending definition (coalescing keeps one request in
  // flight, and the def follows the cursor, so a reply can only be
  // matched by identity, never by position).
  const lastSentKeyRef = useRef<string | null>(null);
  useEffect(() => {
    if (pending) {
      lastSentKeyRef.current = pending.key;
    }
  }, [pending]);

  // When a preview reply answers the pending definition it was sent
  // for, learn the orientation's true origin→min offset: if it
  // differs from what the def assumed, cache it and bump the
  // correction tick (the ghost frame + commit use the corrected
  // offset — no re-send loop, the App skips previews whose key is
  // already cached).  The reply's min−origin is position-independent
  // (linear flatten), so the learning survives the cursor's movement
  // between send and reply.
  useEffect(() => {
    if (!previewReply || !pending) {
      return;
    }
    if (lastSentKeyRef.current !== pending.key) {
      return; // a reply to a superseded sector/orientation — ignore
    }
    const learned: [number, number] = [
      previewReply.view.min[0] - previewReply.view.origin[0],
      previewReply.view.min[1] - previewReply.view.origin[1],
    ];
    const size: [number, number] = [
      previewReply.view.max[0] - previewReply.view.min[0],
      previewReply.view.max[1] - previewReply.view.min[1],
    ];
    const entry = placementCacheRef.current.get(pending.key);
    if (
      !entry?.corrected &&
      (Math.abs(learned[0] - pending.estimate[0]) > OFFSET_TOLERANCE_MM ||
        Math.abs(learned[1] - pending.estimate[1]) > OFFSET_TOLERANCE_MM)
    ) {
      placementCacheRef.current.set(pending.key, {
        offset: learned,
        size,
        corrected: true,
      });
      setCorrectionTick((tick) => tick + 1);
    } else if (!entry) {
      // Within tolerance — just record the learned values for later
      // commits (no re-send needed).
      placementCacheRef.current.set(pending.key, {
        offset: learned,
        size,
        corrected: false,
      });
    }
  }, [previewReply, pending]);

  // The App sends ONE preview per orientation/sector: the entry's
  // presence means the content curves + learned size are in hand —
  // position-only cursor moves render locally (ghost frame + content
  // translation), so they never re-send.
  const previewNeeded = useMemo(() => {
    if (!pending) {
      return false;
    }
    return !placementCacheRef.current.has(pending.key);
  }, [pending]);

  // ── Commit dispatch ──────────────────────────────────────────────

  const handleCommit = useCallback(
    (point: [number, number]) => {
      if (tool === "base_view") {
        // Commit at the click, using the learned offset/size so the
        // placed content lands exactly where the ghost showed it.
        const entry = placementCacheRef.current.get(baseKey);
        const estimate = entry?.offset ?? [0, 0];
        const size = entry?.size ?? [0, 0];
        const clamped = clampSheetPosition(
          point,
          size[0],
          size[1],
          sheetSize.width_mm,
          sheetSize.height_mm,
        );
        const sheetPosition: [number, number] = [
          clamped[0] - estimate[0],
          clamped[1] - estimate[1],
        ];
        const def = pending?.def ?? null;
        if (def) {
          const committed: DrawingView = { ...def, sheet_position: sheetPosition };
          void callbacksRef.current.onCreateBase(committed);
        }
      } else if (tool === "projected_view") {
        const parentId = projectedParent?.viewId;
        const def = pending?.def ?? null;
        if (parentId && def) {
          void callbacksRef.current.onCreateProjected(def, parentId);
        }
      }
    },
    [tool, baseKey, pending, projectedParent, sheetSize],
  );

  // Delete key (delete_view tool): the frame click selected the view.
  useEffect(() => {
    if (tool !== "delete_view" || !selectedViewId) {
      return;
    }
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key !== "Delete" && event.key !== "Backspace") {
        return;
      }
      const target = event.target as HTMLElement | null;
      if (
        target &&
        (target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.isContentEditable)
      ) {
        return;
      }
      event.preventDefault();
      void deleteSelectedView();
    };
    window.addEventListener("keydown", onKeyDown);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
    };
  }, [tool, selectedViewId, deleteSelectedView]);

  return {
    tool,
    base,
    projectedParent,
    selectedViewId,
    previewDefinition: pending?.def ?? null,
    previewNeeded,
    ghostFrame: pending?.ghost ?? null,
    activeSector: pending?.sector ?? null,
    armBaseView,
    armProjectedView,
    armProjectedFrom,
    armSection,
    armMove,
    armDeleteView,
    cancel,
    setOrientation,
    setCurrent3dFrame,
    setScale,
    setShowHidden,
    setBodyChoice,
    pickProjectedParent,
    handleCommit,
    selectViewForDelete,
    deleteSelectedView,
    reset,
  };
}

// ── Key builders (module-local) ────────────────────────────────────

function projectedKey(
  parent: ProjectedParent,
  sectorIndex: number,
  angle: "first_angle" | "third_angle",
): string {
  return [
    parent.viewId,
    sectorIndex,
    parent.bounds.origin[0].toFixed(3),
    parent.bounds.origin[1].toFixed(3),
    parent.scale,
    parent.showHidden ? 1 : 0,
    angle,
  ].join(":");
}
