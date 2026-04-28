import { useEffect, useMemo, useRef, useState } from "react";
import type { DocumentState } from "@/types";

export type CategoryId = "origin" | "sketches" | "bodies";

interface DocumentHierarchyPanelProps {
  document: DocumentState | null;
  hiddenFeatureIds: ReadonlySet<string>;
  hiddenCategories: ReadonlySet<CategoryId>;
  onToggleFeatureVisibility: (featureId: string) => void;
  onToggleCategoryVisibility: (category: CategoryId) => void;
  onSelectFeature: (featureId: string) => Promise<void>;
  onReenterSketch: (featureId: string) => Promise<void>;
  onRenameFeature: (featureId: string, name: string) => Promise<void>;
  onDeleteFeature: (featureId: string) => Promise<void>;
}

interface ContextMenuState {
  x: number;
  y: number;
  featureId: string;
  featureName: string;
  isHidden: boolean;
}

const BODY_KINDS = new Set(["box", "cylinder", "polygon_extrude", "extrude"]);

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

function SketchIcon() {
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
      <path d="M2.5 13.5l3-9 3 4 2-2 3 7" />
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

interface RowProps {
  icon: React.ReactNode;
  label: string;
  isSelected?: boolean;
  isHidden?: boolean;
  isRenaming?: boolean;
  onSelect?: () => void;
  onActivate?: () => void;
  onToggleVisibility?: () => void;
  onContextMenu?: (event: React.MouseEvent) => void;
  onRenameSubmit?: (nextName: string) => void;
  onRenameCancel?: () => void;
  rightContent?: React.ReactNode;
}

function Row({
  icon,
  label,
  isSelected,
  isHidden,
  isRenaming,
  onSelect,
  onActivate,
  onToggleVisibility,
  onContextMenu,
  onRenameSubmit,
  onRenameCancel,
  rightContent,
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
      } ${isHidden ? "opacity-50" : ""}`}
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
        <span className="min-w-0 flex-1 truncate">{label}</span>
      )}
      {rightContent}
      {onToggleVisibility ? (
        <button
          type="button"
          className="flex h-5 w-5 shrink-0 items-center justify-center rounded text-on-surface-dim opacity-0 transition-opacity hover:text-on-surface group-hover:opacity-100"
          style={isHidden ? { opacity: 1 } : undefined}
          onClick={(event) => {
            event.stopPropagation();
            onToggleVisibility();
          }}
          aria-label={isHidden ? "Show" : "Hide"}
          title={isHidden ? "Show" : "Hide"}
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
}

function Category({
  label,
  isOpen,
  onToggleOpen,
  isHidden,
  onToggleVisibility,
  children,
  emptyHint,
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
          aria-label={isHidden ? "Show category" : "Hide category"}
          title={isHidden ? "Show category" : "Hide category"}
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
  onReenterSketch,
  onRenameFeature,
  onDeleteFeature,
}: DocumentHierarchyPanelProps) {
  const [openCategories, setOpenCategories] = useState<Set<CategoryId>>(
    () => new Set<CategoryId>(["origin", "sketches", "bodies"]),
  );
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null);
  const [renamingFeatureId, setRenamingFeatureId] = useState<string | null>(
    null,
  );

  // Dismiss the context menu on any outside click or Escape key.
  useEffect(() => {
    if (!contextMenu) {
      return undefined;
    }
    const dismiss = () => setContextMenu(null);
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        setContextMenu(null);
      }
    };
    window.addEventListener("click", dismiss);
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("click", dismiss);
      window.removeEventListener("keydown", onKey);
    };
  }, [contextMenu]);

  const features = document?.feature_history ?? [];
  const sketches = useMemo(
    () => features.filter((feature) => feature.kind === "sketch"),
    [features],
  );
  const bodies = useMemo(
    () => features.filter((feature) => BODY_KINDS.has(feature.kind)),
    [features],
  );

  if (!document) {
    return (
      <section className="flex h-full flex-col overflow-hidden px-3 py-3">
        <p className="cad-kicker">Browser</p>
        <p className="mt-3 text-sm text-on-surface-muted">
          No active document.
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
    (featureId: string, featureName: string, isHidden: boolean) =>
    (event: React.MouseEvent) => {
      event.preventDefault();
      event.stopPropagation();
      setContextMenu({
        x: event.clientX,
        y: event.clientY,
        featureId,
        featureName,
        isHidden,
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

  return (
    <section className="cad-scrollbar relative flex h-full min-h-0 flex-col overflow-y-auto px-2 py-2">
      <Category
        id="origin"
        label="Origin"
        isOpen={openCategories.has("origin")}
        onToggleOpen={() => toggleOpen("origin")}
        isHidden={hiddenCategories.has("origin")}
        onToggleVisibility={() => onToggleCategoryVisibility("origin")}
      >
        {ORIGIN_PLANES.map((plane) => (
          <Row
            key={plane.id}
            icon={<PlaneIcon />}
            label={plane.label}
            isSelected={document.selected_reference_id === plane.id}
          />
        ))}
        {ORIGIN_AXES.map((axis) => (
          <Row key={axis.id} icon={<AxisIcon />} label={axis.label} />
        ))}
      </Category>

      <Category
        id="sketches"
        label="Sketches"
        isOpen={openCategories.has("sketches")}
        onToggleOpen={() => toggleOpen("sketches")}
        isHidden={hiddenCategories.has("sketches")}
        onToggleVisibility={() => onToggleCategoryVisibility("sketches")}
        emptyHint={sketches.length === 0 ? "No sketches yet" : undefined}
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
              )}
              onRenameSubmit={(nextName) => {
                void submitRename(sketch.feature_id, nextName);
              }}
              onRenameCancel={cancelRename}
            />
          );
        })}
      </Category>

      <Category
        id="bodies"
        label="Bodies"
        isOpen={openCategories.has("bodies")}
        onToggleOpen={() => toggleOpen("bodies")}
        isHidden={hiddenCategories.has("bodies")}
        onToggleVisibility={() => onToggleCategoryVisibility("bodies")}
        emptyHint={bodies.length === 0 ? "No bodies yet" : undefined}
      >
        {bodies.map((body) => {
          const isHidden = hiddenFeatureIds.has(body.feature_id);
          return (
            <Row
              key={body.feature_id}
              icon={<BodyIcon />}
              label={body.name}
              isSelected={document.selected_feature_id === body.feature_id}
              isHidden={isHidden}
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
              )}
              onRenameSubmit={(nextName) => {
                void submitRename(body.feature_id, nextName);
              }}
              onRenameCancel={cancelRename}
            />
          );
        })}
      </Category>

      {contextMenu ? (
        <div
          className="cad-context-menu fixed z-30 min-w-[140px] rounded-xl p-1 shadow-[0_8px_24px_rgba(0,0,0,0.5)] backdrop-blur-xl"
          style={{ left: contextMenu.x, top: contextMenu.y }}
          // Stop propagation so the global dismiss listener does not close
          // the menu before the click on a button is handled.
          onClick={(event) => event.stopPropagation()}
        >
          <button
            type="button"
            className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
            onClick={() => {
              startRename(contextMenu.featureId);
            }}
          >
            Rename
          </button>
          <button
            type="button"
            className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
            onClick={() => {
              onToggleFeatureVisibility(contextMenu.featureId);
              setContextMenu(null);
            }}
          >
            {contextMenu.isHidden ? "Show" : "Hide"}
          </button>
          <button
            type="button"
            className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-red-300 transition-colors hover:bg-red-500/15"
            onClick={() => {
              const id = contextMenu.featureId;
              setContextMenu(null);
              void onDeleteFeature(id);
            }}
          >
            Delete
          </button>
        </div>
      ) : null}
    </section>
  );
}
