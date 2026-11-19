import { Fragment, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useTranslation } from "react-i18next";
import type { DocumentState } from "@/types";
import { ContextMenuShell } from "./ContextMenuShell";
import {
  componentAncestors,
  componentBodyIds,
  componentHierarchy,
  type ComponentTreeNode,
  type HierarchyComponent,
} from "./documentHierarchyGroups";
import {
  blockEndingAtFeatureCount,
  featureCountBeforeBlock,
  featureTimelineBlocks,
} from "./featureTimelineBlocks";
import {
  BoxIcon,
  ComponentIcon,
  FeatureKindIcon,
} from "./header/ToolBarIcons";
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

type TimelineFeature = DocumentState["feature_history"][number];

interface TimelineComponentNodeProps {
  node: ComponentTreeNode<HierarchyComponent>;
  expandedIds: ReadonlySet<string>;
  onToggleExpand: (componentId: string) => void;
  onSelectFeature: (featureId: string) => void;
  features: ReadonlyMap<string, TimelineFeature>;
}

// One component OCCURRENCE of an imported assembly, inside the timeline strip.
//
// An imported board is ONE history operation (an operation is atomic: the cursor
// can sit before or after it, never inside), but it is not one thing the user
// can work with. The file's own occurrences are laid out here in pre-order,
// expanded in place, so the parts are reachable from the timeline the way the
// Browser shows them — without the import becoming 1,092 history entries.
//
// Folding, not nesting by indentation: the strip is one row high, so a child
// group is drawn immediately after its parent inside a bracketed run.
function TimelineComponentNode({
  node,
  expandedIds,
  onToggleExpand,
  onSelectFeature,
  features,
}: TimelineComponentNodeProps) {
  const { t } = useTranslation();
  const memberIds = componentBodyIds(node);
  // A part that is more than one body (a footprint with its pads, a stepped
  // housing) must open, or only its first body would be reachable from the
  // strip. A part with exactly one body needs no second icon.
  const expandable = node.children.length > 0 || node.bodyIds.length > 1;
  const expanded = expandedIds.has(node.component.component_id);
  const name = node.component.name || t("document.unnamedComponent");
  const tooltip = `${name} · ${t("timeline.componentBodies", {
    count: memberIds.length,
  })}`;
  return (
    <>
      <button
        type="button"
        className="cad-icon-button cad-icon-tool h-8 w-8 p-0"
        data-tooltip={tooltip}
        aria-label={tooltip}
        onClick={() => {
          if (expandable) {
            onToggleExpand(node.component.component_id);
            return;
          }
          const [firstBody] = node.bodyIds;
          if (firstBody) {
            onSelectFeature(firstBody);
          }
        }}
      >
        <ComponentIcon className="h-5 w-5" />
      </button>
      {expandable ? (
        <button
          type="button"
          className="flex h-4 w-3 shrink-0 items-center justify-center rounded text-[10px] text-on-surface-dim hover:text-on-surface"
          aria-label={
            expanded
              ? t("timeline.hideComponents")
              : t("timeline.showComponents")
          }
          title={
            expanded
              ? t("timeline.hideComponents")
              : t("timeline.showComponents")
          }
          onClick={() => onToggleExpand(node.component.component_id)}
        >
          {expanded ? "▾" : "▸"}
        </button>
      ) : null}
      {expanded ? (
        <span className="flex items-center gap-0.5 rounded-md px-0.5 ring-1 ring-white/10">
          {node.children.map((child) => (
            <TimelineComponentNode
              key={child.component.component_id}
              node={child}
              expandedIds={expandedIds}
              onToggleExpand={onToggleExpand}
              onSelectFeature={onSelectFeature}
              features={features}
            />
          ))}
          {node.bodyIds.map((bodyId) => {
            const body = features.get(bodyId);
            if (!body) {
              return null;
            }
            return (
              <button
                key={bodyId}
                type="button"
                className="cad-icon-button cad-icon-tool h-8 w-8 p-0"
                data-feature-id={bodyId}
                data-tooltip={body.name}
                aria-label={body.name}
                onClick={() => onSelectFeature(bodyId)}
              >
                <BoxIcon />
              </button>
            );
          })}
        </span>
      ) : null}
    </>
  );
}

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
  // Component occurrences whose children are open in the strip. An import's
  // components are shown as soon as it exists (that is the structure the file
  // brought); each occurrence's own children open on demand, so a board does not
  // put 536 rows of icons on screen at once.
  const [expandedComponents, setExpandedComponents] = useState<Set<string>>(
    () => new Set(),
  );
  // Imports the user folded back to a single block.
  const [collapsedImports, setCollapsedImports] = useState<Set<string>>(
    () => new Set(),
  );

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

  // "Find in Timeline" (viewport right-click) selects a feature; the timeline
  // has to bring it into view, which a long project would otherwise hide
  // off-screen. The row is looked up by its `data-feature-id` rather than held
  // in a ref because a body of an imported assembly lives inside a component
  // group that may not be rendered yet.
  //
  // Both hooks live ABOVE the `!document` early return below. Placed after it
  // they were skipped whenever the document was null and ran on the first
  // document, which React rejects with "Rendered more hooks than during the
  // previous render" and then unmounts the whole UI.
  const stripRef = useRef<HTMLDivElement | null>(null);
  const selectedFeatureId = document?.selected_feature_id ?? null;
  const documentComponents = useMemo(
    () => document?.components ?? [],
    [document],
  );
  useEffect(() => {
    if (!selectedFeatureId) {
      return;
    }
    // The row for a body exists only once the occurrences holding it are open,
    // so open that chain first. Returning `current` when it is already open is
    // what keeps this from re-rendering forever.
    const ancestors = componentAncestors(documentComponents, selectedFeatureId);
    if (ancestors.length > 0) {
      setExpandedComponents((current) => {
        if (ancestors.every((id) => current.has(id))) {
          return current;
        }
        return new Set([...current, ...ancestors]);
      });
    }
    stripRef.current
      ?.querySelector(`[data-feature-id="${selectedFeatureId}"]`)
      ?.scrollIntoView({ block: "nearest", inline: "center" });
  }, [selectedFeatureId, documentComponents, expandedComponents]);

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
  const components = document.components ?? [];
  // Bodies of the imported files, by id: an occurrence's leaves are body
  // features and the strip shows each of them once.
  const featureById = new Map(
    features.map((feature) => [feature.feature_id, feature]),
  );
  const toggleComponentExpand = (componentId: string) => {
    setExpandedComponents((current) => {
      const next = new Set(current);
      if (next.has(componentId)) {
        next.delete(componentId);
      } else {
        next.add(componentId);
      }
      return next;
    });
  };
  const toggleImportCollapsed = (operationId: string) => {
    setCollapsedImports((current) => {
      const next = new Set(current);
      if (next.has(operationId)) {
        next.delete(operationId);
      } else {
        next.add(operationId);
      }
      return next;
    });
  };
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
      <div
        ref={stripRef}
        className="cad-scrollbar flex items-center overflow-x-auto pb-1"
      >
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
          // An imported assembly is ONE operation (the importer stamps the
          // shared operation id), so its row must not introduce itself with the
          // first part's name — a STEP file's unnamed solids are called after
          // the translator. Read it as the import it is.
          const importGroup =
            block.members.length > 1 &&
            block.members.every((member) => member.kind.endsWith("_import"));
          // Tooltip carries both the human-readable name (e.g. "Box 2")
          // and the kind, plus a (suppressed) suffix so the user knows
          // why the feature is missing from the viewport. When a
          // dependency is broken we surface the warning text from the
          // core verbatim — that's the signal that lets the user fix
          // it (e.g. "Sketch plane references a face that no longer
          // exists on body 'feature-2'.").
          const baseTooltip = importGroup
            ? t("timeline.importedAssembly", { count: block.members.length })
            : feature.name && feature.name !== feature.kind
              ? `${feature.name} (${feature.kind})`
              : feature.kind;
          let tooltip = baseTooltip;
          if (block.members.length > 1 && !importGroup) {
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
          // The imported file's component occurrences, when it has any: the
          // strip shows them inside this one operation rather than as extra
          // history entries (an operation is atomic — the cursor cannot sit
          // inside it).
          const hierarchy = importGroup
            ? componentHierarchy(
                components,
                block.members.map((member) => member.feature_id),
              )
            : null;
          const hasComponents = (hierarchy?.forest.length ?? 0) > 0;
          const componentsExpanded = !collapsedImports.has(feature.feature_id);
          // Bodies of the file no component places (a surface body): they stay
          // next to the components, not inside any of them.
          const looseMembers = hierarchy
            ? block.members.filter(
                (member) => !hierarchy.placed.has(member.feature_id),
              )
            : block.members;
          const selectionInsideComponents =
            hierarchy !== null &&
            selectedFeatureId !== null &&
            hierarchy.placed.has(selectedFeatureId);
          const blockActive =
            active && !(hasComponents && componentsExpanded && selectionInsideComponents);
          const blockButton = (
            <button
              type="button"
              data-feature-id={hasComponents ? undefined : feature.feature_id}
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
                  featureIds: block.members.map((member) => member.feature_id),
                  featureName: feature.name,
                  kind: feature.kind,
                  suppressed,
                });
              }}
              className={
                (blockActive
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
          );
          return (
            <Fragment key={feature.feature_id}>
              {renderCursorSlot(featureCountBeforeBlock(blocks, index))}
              {hasComponents ? (
                <span className="flex items-center gap-1">
                  {blockButton}
                  <button
                    type="button"
                    className="flex h-4 w-3 shrink-0 items-center justify-center rounded text-[10px] text-on-surface-dim hover:text-on-surface"
                    aria-label={
                      componentsExpanded
                        ? t("timeline.hideComponents")
                        : t("timeline.showComponents")
                    }
                    title={
                      componentsExpanded
                        ? t("timeline.hideComponents")
                        : t("timeline.showComponents")
                    }
                    onClick={() => toggleImportCollapsed(feature.feature_id)}
                  >
                    {componentsExpanded ? "▾" : "▸"}
                  </button>
                  {componentsExpanded ? (
                    <span className="flex items-center gap-1 rounded-md px-1 ring-1 ring-white/10">
                      {hierarchy.forest.map((node) => (
                        <TimelineComponentNode
                          key={node.component.component_id}
                          node={node}
                          expandedIds={expandedComponents}
                          onToggleExpand={toggleComponentExpand}
                          onSelectFeature={(featureId) => {
                            void onSelectFeature(featureId);
                          }}
                          features={featureById}
                        />
                      ))}
                      {looseMembers.map((member) => (
                        <button
                          key={member.feature_id}
                          type="button"
                          className="cad-icon-button cad-icon-tool h-8 w-8 p-0"
                          data-feature-id={member.feature_id}
                          data-tooltip={member.name}
                          aria-label={member.name}
                          onClick={() => {
                            void onSelectFeature(member.feature_id);
                          }}
                        >
                          <BoxIcon />
                        </button>
                      ))}
                    </span>
                  ) : null}
                </span>
              ) : (
                blockButton
              )}
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
                    // The core deletes an operation atomically, so one call is
                    // enough; calling it per member would throw on the second
                    // ("feature not found") because the first already removed
                    // the whole operation.
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
