import { useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { useTranslation } from "react-i18next";
import type { DocumentState } from "@/types";
import { ContextMenuShell } from "./ContextMenuShell";
import { useContextMenuDismiss } from "./hooks/useContextMenuDismiss";

export type CategoryId = "origin" | "construction" | "sketches" | "bodies";

interface DocumentHierarchyPanelProps {
  document: DocumentState | null;
  hiddenFeatureIds: ReadonlySet<string>;
  hiddenCategories: ReadonlySet<CategoryId>;
  onToggleFeatureVisibility: (featureId: string) => void;
  onToggleCategoryVisibility: (category: CategoryId) => void;
  onSelectFeature: (featureId: string) => Promise<void>;
  // Construction planes are *both* features and references. Clicking
  // one in the hierarchy sets selected_reference_id (so the Sketch
  // button enables) instead of selected_feature_id alone — different
  // dispatch from the timeline / body rows.
  onSelectReference: (referenceId: string) => Promise<void>;
  onReenterSketch: (featureId: string) => Promise<void>;
  onRenameFeature: (featureId: string, name: string) => Promise<void>;
  onDeleteFeature: (featureId: string) => Promise<void>;
  onMoveBody?: (bodyId: string) => Promise<void> | void;
  onCopyBody?: (
    bodyId: string,
    copyMode: "linked" | "standalone",
  ) => Promise<void> | void;
  onExportBodyMesh?: (bodyId: string) => Promise<void> | void;
  onUnlinkBodyCopy?: (featureId: string) => Promise<void> | void;
  // Optional toggle for the persisted suppressed flag (Phase B). When
  // omitted (e.g. read-only previews) the menu just hides the entry.
  onSetFeatureSuppressed?: (
    featureId: string,
    suppressed: boolean,
  ) => Promise<void>;
}

interface ContextMenuState {
  x: number;
  y: number;
  featureId: string;
  featureName: string;
  isHidden: boolean;
  isBody: boolean;
  // Mirrors the feature's persisted `suppressed` flag at the moment
  // the menu was opened. Used to label the Suppress / Unsuppress
  // entry; the persisted state may change after we open the menu, but
  // re-opening rebuilds it.
  suppressed: boolean;
}

const BODY_KINDS = new Set([
  "box",
  "cylinder",
  "polygon_extrude",
  "extrude",
  "loft",
  "revolve",
  "sweep",
  "fastener",
  "body_copy",
  "plugin_feature",
]);

function EyeIcon({ open }: { open: boolean }) {
  if (open) {
    return (
      <svg
        viewBox="0 0 16 16"
        width="14"
        height="14"
        aria-hidden="true"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.4"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        <path d="M1.5 8s2.4-4.5 6.5-4.5S14.5 8 14.5 8 12.1 12.5 8 12.5 1.5 8 1.5 8Z" />
        <circle cx="8" cy="8" r="2" />
      </svg>
    );
  }
  return (
    <svg
      viewBox="0 0 16 16"
      width="14"
      height="14"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M2 2l12 12" />
      <path d="M3.2 5.2C2 6.5 1.5 8 1.5 8s2.4 4.5 6.5 4.5c1.3 0 2.4-.4 3.4-1" />
      <path d="M6.5 4c.5-.2 1-.3 1.5-.3 4.1 0 6.5 4.5 6.5 4.5s-.7 1.4-2 2.6" />
    </svg>
  );
}

function ChevronIcon({ open }: { open: boolean }) {
  return (
    <svg
      viewBox="0 0 16 16"
      width="10"
      height="10"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      style={{
        transform: open ? "rotate(90deg)" : "rotate(0deg)",
        transition: "transform 120ms ease",
      }}
    >
      <path d="M5 3l6 5-6 5" />
    </svg>
  );
}

// Pencil icon, matches the sketch glyph used in the feature timeline
// (`SketchIcon` in `header/ToolBarIcons.tsx`). Drawn here at 12x12 for
// the hierarchy row's tighter vertical rhythm rather than reusing the
// h-5 toolbar version directly.
function SketchIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="12"
      height="12"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M4 20 6 14 16 4l4 4L10 18Z" />
      <path d="m14 6 4 4" />
    </svg>
  );
}

function BodyIcon() {
  return (
    <svg
      viewBox="0 0 16 16"
      width="12"
      height="12"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M8 2 14 5v6L8 14 2 11V5Z" />
      <path d="M2 5l6 3 6-3" />
      <path d="M8 8v6" />
    </svg>
  );
}

function PlaneIcon() {
  return (
    <svg
      viewBox="0 0 16 16"
      width="12"
      height="12"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M2.5 5.5 8 2l5.5 3.5L8 9Z" />
      <path d="M2.5 5.5V11L8 14.5l5.5-3.5V5.5" />
    </svg>
  );
}

function AxisIcon() {
  return (
    <svg
      viewBox="0 0 16 16"
      width="12"
      height="12"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M3 13L13 3" />
      <path d="M9 3h4v4" />
    </svg>
  );
}

function PointIcon() {
  return (
    <svg
      viewBox="0 0 16 16"
      width="12"
      height="12"
      aria-hidden="true"
      fill="currentColor"
    >
      <circle cx="8" cy="8" r="2.5" />
    </svg>
  );
}

function WarningIcon() {
  return (
    <svg
      viewBox="0 0 16 16"
      width="13"
      height="13"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M8 2 14 13H2Z" />
      <path d="M8 6v3" />
      <path d="M8 11.5h.01" />
    </svg>
  );
}

interface RowProps {
  icon: React.ReactNode;
  label: string;
  isSelected?: boolean;
  isHidden?: boolean;
  hasWarning?: boolean;
  warningText?: string;
  isRenaming?: boolean;
  onSelect?: () => void;
  onActivate?: () => void;
  onToggleVisibility?: () => void;
  onContextMenu?: (event: React.MouseEvent) => void;
  onRenameSubmit?: (nextName: string) => void;
  onRenameCancel?: () => void;
  rightContent?: React.ReactNode;
  showLabel: string;
  hideLabel: string;
  needsAttentionLabel: string;
  secondaryLabel?: string | null;
}

function Row({
  icon,
  label,
  isSelected,
  isHidden,
  hasWarning,
  warningText,
  isRenaming,
  onSelect,
  onActivate,
  onToggleVisibility,
  onContextMenu,
  onRenameSubmit,
  onRenameCancel,
  rightContent,
  showLabel,
  hideLabel,
  needsAttentionLabel,
  secondaryLabel,
}: RowProps) {
  const [draft, setDraft] = useState(label);
  const inputRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    if (isRenaming) {
      setDraft(label);
      // Defer focus until after the input has rendered so click events do
      // not steal focus back to the row.
      const id = window.setTimeout(() => {
        inputRef.current?.focus();
        inputRef.current?.select();
      }, 0);
      return () => window.clearTimeout(id);
    }
    return undefined;
  }, [isRenaming, label]);

  return (
    <div
      className={`group flex items-center gap-1.5 rounded-md px-1.5 py-1 text-sm transition-colors ${
        isSelected
          ? "bg-white/10 text-on-surface"
          : "text-on-surface-muted hover:bg-white/[0.04]"
      } ${hasWarning && !isHidden ? "ring-1 ring-amber-400/60 text-amber-200" : ""} ${isHidden ? "opacity-50" : ""}`}
      onClick={isRenaming ? undefined : onSelect}
      onDoubleClick={isRenaming ? undefined : onActivate}
      onContextMenu={onContextMenu}
      role="button"
      tabIndex={0}
    >
      <span className="flex h-4 w-4 shrink-0 items-center justify-center text-on-surface-dim">
        {icon}
      </span>
      {isRenaming ? (
        <input
          ref={inputRef}
          className="min-w-0 flex-1 rounded bg-black/40 px-1 text-sm text-on-surface outline-none ring-1 ring-white/15 focus:ring-white/40"
          value={draft}
          onChange={(event) => {
            setDraft(event.target.value);
          }}
          onClick={(event) => {
            event.stopPropagation();
          }}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              event.preventDefault();
              const trimmed = draft.trim();
              if (trimmed.length === 0 || trimmed === label) {
                onRenameCancel?.();
                return;
              }
              onRenameSubmit?.(trimmed);
            } else if (event.key === "Escape") {
              event.preventDefault();
              onRenameCancel?.();
            }
          }}
          onBlur={() => {
            onRenameCancel?.();
          }}
        />
      ) : (
        <span className="min-w-0 flex-1 truncate">
          {label}
          {secondaryLabel ? (
            <span className="ml-1 text-on-surface-dim">({secondaryLabel})</span>
          ) : null}
        </span>
      )}
      {rightContent}
      {hasWarning ? (
        <span
          className="flex h-5 w-5 shrink-0 items-center justify-center text-amber-300"
          title={warningText || needsAttentionLabel}
          aria-label={warningText || needsAttentionLabel}
        >
          <WarningIcon />
        </span>
      ) : null}
      {onToggleVisibility ? (
        <button
          type="button"
          className="flex h-5 w-5 shrink-0 items-center justify-center rounded text-on-surface-dim opacity-0 transition-opacity hover:text-on-surface group-hover:opacity-100"
          style={isHidden ? { opacity: 1 } : undefined}
          onClick={(event) => {
            event.stopPropagation();
            onToggleVisibility();
          }}
          aria-label={isHidden ? showLabel : hideLabel}
          title={isHidden ? showLabel : hideLabel}
        >
          <EyeIcon open={!isHidden} />
        </button>
      ) : null}
    </div>
  );
}

interface CategoryProps {
  id: CategoryId;
  label: string;
  isOpen: boolean;
  onToggleOpen: () => void;
  isHidden: boolean;
  onToggleVisibility: () => void;
  children: React.ReactNode;
  emptyHint?: string;
  showCategoryLabel: string;
  hideCategoryLabel: string;
}

function Category({
  label,
  isOpen,
  onToggleOpen,
  isHidden,
  onToggleVisibility,
  children,
  emptyHint,
  showCategoryLabel,
  hideCategoryLabel,
}: CategoryProps) {
  return (
    <div className="select-none">
      <div
        className={`group flex items-center gap-1.5 rounded-md px-1 py-1 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim transition-colors hover:bg-white/[0.04] ${
          isHidden ? "opacity-50" : ""
        }`}
        role="button"
        tabIndex={0}
        onClick={onToggleOpen}
      >
        <span className="flex h-4 w-4 shrink-0 items-center justify-center">
          <ChevronIcon open={isOpen} />
        </span>
        <span className="flex-1">{label}</span>
        <button
          type="button"
          className="flex h-5 w-5 shrink-0 items-center justify-center rounded text-on-surface-dim opacity-0 transition-opacity hover:text-on-surface group-hover:opacity-100"
          style={isHidden ? { opacity: 1 } : undefined}
          onClick={(event) => {
            event.stopPropagation();
            onToggleVisibility();
          }}
          aria-label={isHidden ? showCategoryLabel : hideCategoryLabel}
          title={isHidden ? showCategoryLabel : hideCategoryLabel}
        >
          <EyeIcon open={!isHidden} />
        </button>
      </div>
      {isOpen ? (
        <div className="ml-4 border-l border-white/5 pl-2">
          {children ?? null}
          {emptyHint ? (
            <p className="px-1.5 py-1 text-xs text-on-surface-dim">
              {emptyHint}
            </p>
          ) : null}
        </div>
      ) : null}
    </div>
  );
}

const ORIGIN_PLANES: Array<{
  id: string;
  label: string;
}> = [
  { id: "ref-plane-xy", label: "XY" },
  { id: "ref-plane-yz", label: "YZ" },
  { id: "ref-plane-xz", label: "XZ" },
];

const ORIGIN_AXES: Array<{ id: string; label: string }> = [
  { id: "ref-axis-x", label: "X" },
  { id: "ref-axis-y", label: "Y" },
  { id: "ref-axis-z", label: "Z" },
];

export function DocumentHierarchyPanel({
  document,
  hiddenFeatureIds,
  hiddenCategories,
  onToggleFeatureVisibility,
  onToggleCategoryVisibility,
  onSelectFeature,
  onSelectReference,
  onReenterSketch,
  onRenameFeature,
  onDeleteFeature,
  onMoveBody,
  onCopyBody,
  onExportBodyMesh,
  onUnlinkBodyCopy,
  onSetFeatureSuppressed,
}: DocumentHierarchyPanelProps) {
  const { t } = useTranslation();
  const [openCategories, setOpenCategories] = useState<Set<CategoryId>>(
    () => new Set<CategoryId>(["origin", "construction", "sketches", "bodies"]),
  );
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null);
  const [renamingFeatureId, setRenamingFeatureId] = useState<string | null>(
    null,
  );

  useContextMenuDismiss(Boolean(contextMenu), () => setContextMenu(null));

  const features = document?.feature_history ?? [];
  const featureNameById = useMemo(
    () =>
      new Map(
        features.map((feature) => [
          feature.feature_id,
          feature.name || feature.kind,
        ]),
      ),
    [features],
  );
  const contextFeature = contextMenu
    ? features.find((feature) => feature.feature_id === contextMenu.featureId)
    : null;
  const contextIsActiveSketch =
    contextFeature?.kind === "sketch" &&
    contextMenu?.featureId === document?.active_sketch_feature_id;
  const contextCanDelete =
    contextFeature?.kind !== "root_part" && !contextIsActiveSketch;
  const contextIsLinkedCopy =
    contextFeature?.kind === "body_copy" &&
    contextFeature.body_copy_parameters?.copy_mode === "linked";
  const sketches = useMemo(
    () => features.filter((feature) => feature.kind === "sketch"),
    [features],
  );
  const constructionFeatures = useMemo(
    () =>
      features.filter((feature) =>
        ["construction_plane", "construction_axis", "construction_point"].includes(
          feature.kind,
        ),
      ),
    [features],
  );
  const bodies = useMemo(
    () =>
      features.filter((feature) => {
        if (!BODY_KINDS.has(feature.kind)) {
          return false;
        }
        // Boolean-mode extrudes (cut / join) don't produce their own
        // body — they get consumed into their target body during
        // compilation. Listing them in the Bodies category would be
        // misleading: there's no separate body for the user to
        // select, hide, or rename. They still appear in the feature
        // timeline (so the user can edit/delete them), they just
        // don't show up here. Box / cylinder / polygon_extrude are
        // always standalone bodies, and a default "new_body" extrude
        // is too.
        if (
          feature.kind === "extrude" &&
          feature.extrude_parameters !== null &&
          feature.extrude_parameters.mode !== "new_body"
        ) {
          return false;
        }
        return true;
      }),
    [features],
  );

  if (!document) {
    return (
      <section className="flex h-full flex-col overflow-hidden px-3 py-3">
        <p className="cad-kicker">{t("common.browser")}</p>
        <p className="mt-3 text-sm text-on-surface-muted">
          {t("document.noActiveDocument")}
        </p>
      </section>
    );
  }

  const toggleOpen = (id: CategoryId) => {
    setOpenCategories((current) => {
      const next = new Set(current);
      if (next.has(id)) {
        next.delete(id);
      } else {
        next.add(id);
      }
      return next;
    });
  };

  const openContextMenu =
    (
      featureId: string,
      featureName: string,
      isHidden: boolean,
      suppressed: boolean,
      isBody: boolean,
    ) =>
    (event: React.MouseEvent) => {
      event.preventDefault();
      event.stopPropagation();
      setContextMenu({
        x: event.clientX,
        y: event.clientY,
        featureId,
        featureName,
        isHidden,
        suppressed,
        isBody,
      });
    };

  const startRename = (featureId: string) => {
    setRenamingFeatureId(featureId);
    setContextMenu(null);
  };

  const submitRename = async (featureId: string, nextName: string) => {
    setRenamingFeatureId(null);
    await onRenameFeature(featureId, nextName);
  };

  const cancelRename = () => {
    setRenamingFeatureId(null);
  };
  const rowLabels = {
    showLabel: t("common.show"),
    hideLabel: t("common.hide"),
    needsAttentionLabel: t("common.needsAttention"),
  };
  const categoryLabels = {
    showCategoryLabel: t("document.showCategory"),
    hideCategoryLabel: t("document.hideCategory"),
  };

  return (
    <section className="cad-scrollbar relative flex h-full min-h-0 flex-col overflow-y-auto px-2 py-2">
      <Category
        id="origin"
        label={t("document.origin")}
        isOpen={openCategories.has("origin")}
        onToggleOpen={() => toggleOpen("origin")}
        isHidden={hiddenCategories.has("origin")}
        onToggleVisibility={() => onToggleCategoryVisibility("origin")}
        {...categoryLabels}
      >
        {ORIGIN_PLANES.map((plane) => (
          <Row
            key={plane.id}
            icon={<PlaneIcon />}
            label={plane.label}
            isSelected={document.selected_reference_id === plane.id}
            {...rowLabels}
          />
        ))}
        {ORIGIN_AXES.map((axis) => (
          <Row key={axis.id} icon={<AxisIcon />} label={axis.label} {...rowLabels} />
        ))}
      </Category>

      <Category
        id="construction"
        label={t("document.construction")}
        isOpen={openCategories.has("construction")}
        onToggleOpen={() => toggleOpen("construction")}
        isHidden={hiddenCategories.has("construction")}
        onToggleVisibility={() => onToggleCategoryVisibility("construction")}
        emptyHint={
          constructionFeatures.length === 0
            ? t("document.noConstructionGeometry")
            : undefined
        }
        {...categoryLabels}
      >
        {constructionFeatures.map((feature) => {
          const isHidden = hiddenFeatureIds.has(feature.feature_id);
          const icon =
            feature.kind === "construction_plane" ? (
              <PlaneIcon />
            ) : feature.kind === "construction_axis" ? (
              <AxisIcon />
            ) : (
              <PointIcon />
            );
          return (
            <Row
              key={feature.feature_id}
              icon={icon}
              label={feature.name}
              // Construction planes get highlighted via either
              // selected_reference_id or selected_feature_id —
              // both selectors are valid for them. Match the
              // viewport's behaviour by light-up if either is set.
              isSelected={
                document.selected_reference_id === feature.feature_id ||
                document.selected_feature_id === feature.feature_id
              }
              isHidden={isHidden}
              hasWarning={feature.dependency_broken === true}
              warningText={feature.dependency_warning}
              isRenaming={renamingFeatureId === feature.feature_id}
              onSelect={() => {
                // Treat construction planes as references when
                // clicked here so the Sketch button enables. The
                // core also sets selected_feature_id internally.
                if (feature.kind === "construction_plane") {
                  void onSelectReference(feature.feature_id);
                  return;
                }
                void onSelectFeature(feature.feature_id);
              }}
              onToggleVisibility={() => {
                onToggleFeatureVisibility(feature.feature_id);
              }}
              onContextMenu={openContextMenu(
                feature.feature_id,
                feature.name,
                isHidden,
                feature.suppressed === true,
                false,
              )}
              onRenameSubmit={(nextName) => {
                void submitRename(feature.feature_id, nextName);
              }}
              onRenameCancel={cancelRename}
              {...rowLabels}
            />
          );
        })}
      </Category>

      <Category
        id="sketches"
        label={t("document.sketches")}
        isOpen={openCategories.has("sketches")}
        onToggleOpen={() => toggleOpen("sketches")}
        isHidden={hiddenCategories.has("sketches")}
        onToggleVisibility={() => onToggleCategoryVisibility("sketches")}
        emptyHint={sketches.length === 0 ? t("document.noSketches") : undefined}
        {...categoryLabels}
      >
        {sketches.map((sketch) => {
          const isHidden = hiddenFeatureIds.has(sketch.feature_id);
          return (
            <Row
              key={sketch.feature_id}
              icon={<SketchIcon />}
              label={sketch.name}
              isSelected={document.selected_feature_id === sketch.feature_id}
              isHidden={isHidden}
              hasWarning={sketch.dependency_broken === true}
              warningText={sketch.dependency_warning}
              isRenaming={renamingFeatureId === sketch.feature_id}
              onSelect={() => {
                void onSelectFeature(sketch.feature_id);
              }}
              onActivate={() => {
                void onReenterSketch(sketch.feature_id);
              }}
              onToggleVisibility={() => {
                onToggleFeatureVisibility(sketch.feature_id);
              }}
              onContextMenu={openContextMenu(
                sketch.feature_id,
                sketch.name,
                isHidden,
                sketch.suppressed === true,
                false,
              )}
              onRenameSubmit={(nextName) => {
                void submitRename(sketch.feature_id, nextName);
              }}
              onRenameCancel={cancelRename}
              {...rowLabels}
            />
          );
        })}
      </Category>

      <Category
        id="bodies"
        label={t("document.bodies")}
        isOpen={openCategories.has("bodies")}
        onToggleOpen={() => toggleOpen("bodies")}
        isHidden={hiddenCategories.has("bodies")}
        onToggleVisibility={() => onToggleCategoryVisibility("bodies")}
        emptyHint={bodies.length === 0 ? t("document.noBodies") : undefined}
        {...categoryLabels}
      >
        {bodies.map((body) => {
          const isHidden = hiddenFeatureIds.has(body.feature_id);
          const linkedCopySource =
            body.kind === "body_copy" &&
            body.body_copy_parameters?.copy_mode === "linked"
              ? (featureNameById.get(body.body_copy_parameters.source_body_id) ??
                body.body_copy_parameters.source_body_name)
              : null;
          return (
            <Row
              key={body.feature_id}
              icon={<BodyIcon />}
              label={body.name}
              secondaryLabel={linkedCopySource}
              isSelected={document.selected_feature_id === body.feature_id}
              isHidden={isHidden}
              hasWarning={body.dependency_broken === true}
              warningText={body.dependency_warning}
              isRenaming={renamingFeatureId === body.feature_id}
              onSelect={() => {
                void onSelectFeature(body.feature_id);
              }}
              onToggleVisibility={() => {
                onToggleFeatureVisibility(body.feature_id);
              }}
              onContextMenu={openContextMenu(
                body.feature_id,
                body.name,
                isHidden,
                body.suppressed === true,
                true,
              )}
              onRenameSubmit={(nextName) => {
                void submitRename(body.feature_id, nextName);
              }}
              onRenameCancel={cancelRename}
              {...rowLabels}
            />
          );
        })}
      </Category>

      {contextMenu
        ? createPortal(
            // Portaled to document.body for the same reason the timeline
            // does it: `.cad-sidebar` has `backdrop-filter` set, which
            // makes it a containing block for fixed-position descendants
            // and skews the menu's anchor.
            //
            // ContextMenuShell handles the auto-flip: when the click
            // is near the bottom / right edge of the viewport the menu
            // anchors its bottom / right to the click point so it
            // stays visible. Same shell as the feature timeline uses.
            <ContextMenuShell
              x={contextMenu.x}
              y={contextMenu.y}
              className="min-w-[140px]"
            >
              <button
                type="button"
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                onClick={() => {
                  startRename(contextMenu.featureId);
                }}
              >
                {t("common.rename")}
              </button>
              {contextMenu.isBody ? (
                <>
                  <button
                    type="button"
                    className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                    onClick={() => {
                      const id = contextMenu.featureId;
                      setContextMenu(null);
                      void onMoveBody?.(id);
                    }}
                  >
                    {t("common.move")}
                  </button>
                  <div className="group/copy relative">
                    <button
                      type="button"
                      className="flex w-full items-center justify-between gap-3 rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                    >
                      <span>{t("common.copy")}</span>
                      <span className="text-on-surface-dim">&gt;</span>
                    </button>
                    <div className="cad-context-menu invisible absolute left-full top-0 z-40 ml-1 min-w-[170px] rounded-xl p-1 opacity-0 shadow-[0_8px_24px_rgba(0,0,0,0.5)] backdrop-blur-xl transition-opacity group-hover/copy:visible group-hover/copy:opacity-100">
                      <button
                        type="button"
                        className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                        onClick={() => {
                          const id = contextMenu.featureId;
                          setContextMenu(null);
                          void onCopyBody?.(id, "linked");
                        }}
                      >
                        {t("common.copyLinked")}
                      </button>
                      <button
                        type="button"
                        className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                        onClick={() => {
                          const id = contextMenu.featureId;
                          setContextMenu(null);
                          void onCopyBody?.(id, "standalone");
                        }}
                      >
                        {t("common.copyIndependent")}
                      </button>
                    </div>
                  </div>
                  {contextIsLinkedCopy ? (
                    <button
                      type="button"
                      className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                      onClick={() => {
                        const id = contextMenu.featureId;
                        setContextMenu(null);
                        void onUnlinkBodyCopy?.(id);
                      }}
                    >
                      {t("common.unlink")}
                    </button>
                  ) : null}
                  <button
                    type="button"
                    className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                    onClick={() => {
                      const id = contextMenu.featureId;
                      setContextMenu(null);
                      void onExportBodyMesh?.(id);
                    }}
                  >
                    {t("common.exportAsMesh")}
                  </button>
                </>
              ) : null}
              <button
                type="button"
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                onClick={() => {
                  onToggleFeatureVisibility(contextMenu.featureId);
                  setContextMenu(null);
                }}
              >
                {contextMenu.isHidden ? t("common.show") : t("common.hide")}
              </button>
              {onSetFeatureSuppressed ? (
                <button
                  type="button"
                  className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                  onClick={() => {
                    const { featureId, suppressed } = contextMenu;
                    setContextMenu(null);
                    void onSetFeatureSuppressed(featureId, !suppressed);
                  }}
                >
                  {contextMenu.suppressed
                    ? t("common.unsuppress")
                    : t("common.suppress")}
                </button>
              ) : null}
              <button
                type="button"
                disabled={!contextCanDelete}
                title={
                  contextIsActiveSketch
                    ? t("timeline.activeSketchDeleteBlocked")
                    : undefined
                }
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-red-300 transition-colors hover:bg-red-500/15 disabled:cursor-not-allowed disabled:text-on-surface-dim disabled:hover:bg-transparent"
                onClick={() => {
                  const id = contextMenu.featureId;
                  setContextMenu(null);
                  void onDeleteFeature(id);
                }}
              >
                {t("common.delete")}
              </button>
            </ContextMenuShell>,
            window.document.body,
          )
        : null}
    </section>
  );
}
