import { useMemo, useState } from "react";
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
  onSelect?: () => void;
  onActivate?: () => void;
  onToggleVisibility?: () => void;
  rightContent?: React.ReactNode;
}

function Row({
  icon,
  label,
  isSelected,
  isHidden,
  onSelect,
  onActivate,
  onToggleVisibility,
  rightContent,
}: RowProps) {
  return (
    <div
      className={`group flex items-center gap-1.5 rounded-md px-1.5 py-1 text-sm transition-colors ${
        isSelected
          ? "bg-white/10 text-on-surface"
          : "text-on-surface-muted hover:bg-white/[0.04]"
      } ${isHidden ? "opacity-50" : ""}`}
      onClick={onSelect}
      onDoubleClick={onActivate}
      role="button"
      tabIndex={0}
    >
      <span className="flex h-4 w-4 shrink-0 items-center justify-center text-on-surface-dim">
        {icon}
      </span>
      <span className="min-w-0 flex-1 truncate">{label}</span>
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
            <p className="px-1.5 py-1 text-xs text-on-surface-dim">{emptyHint}</p>
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
}: DocumentHierarchyPanelProps) {
  const [openCategories, setOpenCategories] = useState<Set<CategoryId>>(
    () => new Set<CategoryId>(["origin", "sketches", "bodies"]),
  );

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
        <p className="mt-3 text-sm text-on-surface-muted">No active document.</p>
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

  return (
    <section className="cad-scrollbar flex h-full min-h-0 flex-col overflow-y-auto px-2 py-2">
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
        {sketches.map((sketch) => (
          <Row
            key={sketch.feature_id}
            icon={<SketchIcon />}
            label={sketch.name}
            isSelected={document.selected_feature_id === sketch.feature_id}
            isHidden={hiddenFeatureIds.has(sketch.feature_id)}
            onSelect={() => {
              void onSelectFeature(sketch.feature_id);
            }}
            onActivate={() => {
              void onReenterSketch(sketch.feature_id);
            }}
            onToggleVisibility={() => {
              onToggleFeatureVisibility(sketch.feature_id);
            }}
          />
        ))}
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
        {bodies.map((body) => (
          <Row
            key={body.feature_id}
            icon={<BodyIcon />}
            label={body.name}
            isSelected={document.selected_feature_id === body.feature_id}
            isHidden={hiddenFeatureIds.has(body.feature_id)}
            onSelect={() => {
              void onSelectFeature(body.feature_id);
            }}
            onToggleVisibility={() => {
              onToggleFeatureVisibility(body.feature_id);
            }}
          />
        ))}
      </Category>
    </section>
  );
}
