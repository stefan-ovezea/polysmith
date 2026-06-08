import { Fragment, useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useTranslation } from "react-i18next";
import type { DocumentState } from "@/types";
import { ContextMenuShell } from "./ContextMenuShell";
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
  const renderCursorSlot = (position: number) => (
    <button
      key={`cursor-${position}`}
      type="button"
      className={
        "cad-tooltip-trigger cad-timeline-cursor-slot" +
        (cursorPosition === position ? " cad-timeline-cursor-slot-active" : "")
      }
      data-tooltip={t("timeline.cursor")}
      aria-label={t("timeline.cursor")}
      onClick={() => setCursor(position)}
      onPointerDown={(event) => {
        event.preventDefault();
        beginCursorDrag(position);
      }}
      onPointerEnter={() => maybeDragTo(position)}
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
        {features.map((feature, index) => {
          const active = feature.feature_id === document.selected_feature_id;
          const suppressed = feature.suppressed === true;
          const dependencyBroken = feature.dependency_broken === true;
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
          if (suppressed) {
            tooltip = `${tooltip} - ${t("timeline.suppressed")}`;
          }
          if (dependencyBroken && feature.dependency_warning) {
            tooltip = `${tooltip}\n⚠ ${feature.dependency_warning}`;
          }
          return (
            <Fragment key={feature.feature_id}>
              {renderCursorSlot(index)}
              <button
                type="button"
                onClick={() => {
                  void onSelectFeature(feature.feature_id);
                }}
                onDoubleClick={() => {
                  setCursor(index + 1);
                  onEditFeature?.(feature.feature_id);
                }}
                onPointerMove={(event) => {
                  if (!draggingCursor) {
                    return;
                  }
                  const rect = event.currentTarget.getBoundingClientRect();
                  maybeDragTo(
                    event.clientX < rect.left + rect.width / 2
                      ? index
                      : index + 1,
                  );
                }}
                onContextMenu={(event) => {
                  event.preventDefault();
                  event.stopPropagation();
                  setContextMenu({
                    x: event.clientX,
                    y: event.clientY,
                    featureId: feature.feature_id,
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
      </div>
      {contextMenu
        ? createPortal(
            <ContextMenuShell x={contextMenu.x} y={contextMenu.y}>
              <button
                type="button"
                disabled={!canEdit || !onEditFeature}
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10 disabled:cursor-not-allowed disabled:text-on-surface-dim disabled:hover:bg-transparent"
                onClick={() => {
                  const id = contextMenu.featureId;
                  const index = features.findIndex(
                    (feature) => feature.feature_id === id,
                  );
                  if (index >= 0) {
                    setCursor(index + 1);
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
                    const { featureId, suppressed } = contextMenu;
                    setContextMenu(null);
                    onSuppressFeature(featureId, !suppressed);
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
                    const id = contextMenu.featureId;
                    setContextMenu(null);
                    onDeleteFeature(id);
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
