import { Fragment, useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useTranslation } from "react-i18next";
import type { DocumentState } from "@/types";
import { ContextMenuShell } from "./ContextMenuShell";
import {
  blockEndingAtFeatureCount,
  featureCountBeforeBlock,
  featureTimelineBlocks,
} from "./featureTimelineBlocks";
import { FeatureKindIcon } from "./header/ToolBarIcons";
import { useContextMenuDismiss } from "./hooks/useContextMenuDismiss";

interface FeatureTimelineProps {
  document: DocumentState | null;
  onSelectFeature: (featureId: string) => Promise<void>;
  // Optional double-click handler. The parent decides which feature
  // kinds are editable; the timeline just forwards the id and lets the
  // parent decide whether to mount an editor. Kept optional so the
  // timeline stays renderable in contexts where editing isn't wired
  // (e.g. read-only previews).
  onEditFeature?: (featureId: string) => void;
  // Optional context-menu actions. The timeline only renders the menu
  // entries whose handlers are provided, so callers can opt into a
  // subset (e.g. read-only previews can omit Suppress + Delete).
  onSuppressFeature?: (featureId: string, suppressed: boolean) => void;
  onDeleteFeature?: (featureId: string) => void;
  onSetTimelineCursor?: (includedActionCount: number) => void;
}

interface ContextMenuState {
  x: number;
  y: number;
  featureId: string;
  // Every feature of the operation the menu was opened on. Edit/suppress/delete
  // act on the whole operation: its rows are one gesture, and deleting half of
  // an extrude would silently leave the remaining bodies behind.
  featureIds: string[];
  featureName: string;
  kind: string;
  suppressed: boolean;
}

// Compact icon-only timeline. Each feature is rendered as a square
// `cad-icon-button` with the feature kind's icon and a tooltip showing
// the full name + kind. The previous version used circle nodes plus
// text labels separated by horizontal bars; that stopped scaling once
// documents grew past ~6 features. Icons are uniform 32×32 with 4px
// gaps so we get ~30 features per 1280px ribbon before the timeline
// has to scroll.
// Kinds that the parent's `onEditFeature` actually opens a panel for.
// The menu still surfaces the entry but disables it (greyed out) for
// non-editable kinds so the user discovers what's coming, rather than
// being silently ignored on click.
// Kinds the parent can do *something* meaningful with on double-click.
// `box` / `cylinder` open their parameter panels; profile-based features open
// their preview panels for live edits; `sketch` re-enters the sketch (same as
// the hierarchy panel's pencil icon).
// The actual dispatch lives in App.tsx's `onEditFeature`.
const EDITABLE_KINDS = new Set([
  "box",
  "cylinder",
  "extrude",
  "loft",
  "revolve",
  "sweep",
  "thread",
  "fastener",
  "move",
  "sketch",
]);

export function FeatureTimeline({
  document,
  onSelectFeature,
  onEditFeature,
  onSuppressFeature,
  onDeleteFeature,
  onSetTimelineCursor,
}: FeatureTimelineProps) {
  const { t } = useTranslation();
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null);
  const [draggingCursor, setDraggingCursor] = useState(false);
  const lastRequestedCursor = useRef<number | null>(null);

  useContextMenuDismiss(Boolean(contextMenu), () => setContextMenu(null), {
    includeContextMenu: true,
  });

  useEffect(() => {
    if (!draggingCursor) {
      return;
    }
    const stop = () => setDraggingCursor(false);
    window.addEventListener("pointerup", stop);
    window.addEventListener("pointercancel", stop);
    return () => {
      window.removeEventListener("pointerup", stop);
      window.removeEventListener("pointercancel", stop);
    };
  }, [draggingCursor]);

  if (!document) {
    return null;
  }

  const features = document.feature_history.filter(
    (feature) => feature.kind !== "root_part",
  );
  // One commit can create several features (one body per disconnected surface).
  // They share `operation_id`, and the timeline shows them as ONE block — the
  // Fusion model — while each body keeps its own feature id for fillets, CAM
  // references and `feature-N:face:M` sketch planes.
  const blocks = featureTimelineBlocks(features);
  const cursorPosition = Math.min(
    Math.max(document.timeline_cursor ?? features.length, 0),
    features.length,
  );
  if (!draggingCursor) {
    lastRequestedCursor.current = null;
  }
  const setCursor = (position: number) => {
    const nextPosition = Math.min(Math.max(position, 0), features.length);
    if (lastRequestedCursor.current === nextPosition) {
      return;
    }
    lastRequestedCursor.current = nextPosition;
    onSetTimelineCursor?.(nextPosition);
  };
  const beginCursorDrag = (position: number) => {
    setDraggingCursor(true);
    setCursor(position);
  };
  const maybeDragTo = (position: number) => {
    if (draggingCursor) {
      setCursor(position);
    }
  };
  // A cursor slot sits at an operation boundary and carries the number of
  // FEATURES it includes (the core's `timeline_cursor` counts features), so an
  // operation is atomic: you can roll back before or after it, never inside it.
  const renderCursorSlot = (includedFeatureCount: number) => (
    <button
      key={`cursor-${includedFeatureCount}`}
      type="button"
      className={
        "cad-tooltip-trigger cad-timeline-cursor-slot" +
        (cursorPosition === includedFeatureCount
          ? " cad-timeline-cursor-slot-active"
          : "")
      }
      data-tooltip={t("timeline.cursor")}
      aria-label={t("timeline.cursor")}
      onClick={() => setCursor(includedFeatureCount)}
      onPointerDown={(event) => {
        event.preventDefault();
        beginCursorDrag(includedFeatureCount);
      }}
      onPointerEnter={() => maybeDragTo(includedFeatureCount)}
    >
      <span className="cad-timeline-cursor-bar" aria-hidden="true" />
    </button>
  );
  const canEdit = contextMenu ? EDITABLE_KINDS.has(contextMenu.kind) : false;
  const canSuppress = contextMenu ? contextMenu.kind !== "root_part" : false;
  const isActiveSketch =
    contextMenu?.kind === "sketch" &&
    contextMenu.featureId === document.active_sketch_feature_id;
  const canDelete = canSuppress && !isActiveSketch;

  return (
    <div className="cad-timeline pointer-events-auto px-4 py-2.5">
      <div className="cad-scrollbar flex items-center overflow-x-auto pb-1">
        {blocks.map((block, index) => {
          const feature = block.primary;
          const active = block.members.some(
            (member) => member.feature_id === document.selected_feature_id,
          );
          const suppressed = block.members.every(
            (member) => member.suppressed === true,
          );
          const dependencyBroken = block.members.some(
            (member) => member.dependency_broken === true,
          );
          const dependencyAmbiguous = block.members.some(
            (member) => member.dependency_ambiguous === true,
          );
          // Tooltip carries both the human-readable name (e.g. "Box 2")
          // and the kind, plus a (suppressed) suffix so the user knows
          // why the feature is missing from the viewport. When a
          // dependency is broken we surface the warning text from the
          // core verbatim — that's the signal that lets the user fix
          // it (e.g. "Sketch plane references a face that no longer
          // exists on body 'feature-2'.").
          const baseTooltip =
            feature.name && feature.name !== feature.kind
              ? `${feature.name} (${feature.kind})`
              : feature.kind;
          let tooltip = baseTooltip;
          if (block.members.length > 1) {
            tooltip = `${tooltip} · ${t("timeline.operationFeatures", {
              count: block.members.length,
            })}`;
          }
          if (suppressed) {
            tooltip = `${tooltip} - ${t("timeline.suppressed")}`;
          }
          if (dependencyBroken && feature.dependency_warning) {
            tooltip = `${tooltip}\n⚠ ${feature.dependency_warning}`;
          }
          if (dependencyAmbiguous && feature.dependency_ambiguous_warning) {
            tooltip = `${tooltip}\n? ${feature.dependency_ambiguous_warning}`;
          }
          return (
            <Fragment key={feature.feature_id}>
              {renderCursorSlot(featureCountBeforeBlock(blocks, index))}
              <button
                type="button"
                onClick={() => {
                  void onSelectFeature(feature.feature_id);
                }}
                onDoubleClick={() => {
                  setCursor(featureCountBeforeBlock(blocks, index + 1));
                  onEditFeature?.(feature.feature_id);
                }}
                onPointerMove={(event) => {
                  if (!draggingCursor) {
                    return;
                  }
                  const rect = event.currentTarget.getBoundingClientRect();
                  maybeDragTo(
                    event.clientX < rect.left + rect.width / 2
                      ? featureCountBeforeBlock(blocks, index)
                      : featureCountBeforeBlock(blocks, index + 1),
                  );
                }}
                onContextMenu={(event) => {
                  event.preventDefault();
                  event.stopPropagation();
                  setContextMenu({
                    x: event.clientX,
                    y: event.clientY,
                    featureId: feature.feature_id,
                    featureIds: block.members.map(
                      (member) => member.feature_id,
                    ),
                    featureName: feature.name,
                    kind: feature.kind,
                    suppressed,
                  });
                }}
                className={
                  (active
                    ? "cad-icon-button cad-icon-tool cad-icon-tool-active h-9 w-9 p-0"
                    : "cad-icon-button cad-icon-tool h-9 w-9 p-0") +
                  (suppressed ? " opacity-40" : "") +
                  (dependencyBroken && !suppressed
                    ? " cad-timeline-feature-warning"
                    : "") +
                  (dependencyAmbiguous && !suppressed && !dependencyBroken
                    ? " cad-timeline-feature-ambiguous"
                    : "")
                }
                data-tooltip={tooltip}
                aria-label={tooltip}
              >
                <FeatureKindIcon kind={feature.kind} />
              </button>
            </Fragment>
          );
        })}
        {renderCursorSlot(features.length)}
      </div>      {contextMenu
        ? createPortal(
            <ContextMenuShell x={contextMenu.x} y={contextMenu.y}>
              <button
                type="button"
                disabled={!canEdit || !onEditFeature}
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10 disabled:cursor-not-allowed disabled:text-on-surface-dim disabled:hover:bg-transparent"
                onClick={() => {
                  const id = contextMenu.featureId;
                  const blockIndex = blocks.findIndex(
                    (block) => block.primary.feature_id === id,
                  );
                  if (blockIndex >= 0) {
                    setCursor(featureCountBeforeBlock(blocks, blockIndex + 1));
                  }
                  setContextMenu(null);
                  onEditFeature?.(id);
                }}
              >
                {t("common.edit")}
              </button>
              {onSuppressFeature ? (
                <button
                  type="button"
                  disabled={!canSuppress}
                  className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10 disabled:cursor-not-allowed disabled:text-on-surface-dim disabled:hover:bg-transparent"
                  onClick={() => {
                    const { featureIds, suppressed } = contextMenu;
                    setContextMenu(null);
                    for (const id of featureIds) {
                      onSuppressFeature(id, !suppressed);
                    }
                  }}
                >
                  {contextMenu.suppressed
                    ? t("common.unsuppress")
                    : t("common.suppress")}
                </button>
              ) : null}
              {onDeleteFeature ? (
                <button
                  type="button"
                  disabled={!canDelete}
                  title={
                    isActiveSketch
                      ? t("timeline.activeSketchDeleteBlocked")
                      : undefined
                  }
                  className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-red-300 transition-colors hover:bg-red-500/15 disabled:cursor-not-allowed disabled:text-on-surface-dim disabled:hover:bg-transparent"
                  onClick={() => {
                    const { featureIds } = contextMenu;
                    setContextMenu(null);
                    for (const id of featureIds) {
                      onDeleteFeature(id);
                    }
                  }}
                >
                  {t("common.delete")}
                </button>
              ) : null}
            </ContextMenuShell>,
            window.document.body,
          )
        : null}
    </div>
  );
}
