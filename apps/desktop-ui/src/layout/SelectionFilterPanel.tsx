import { useEffect, useState, type ReactNode } from "react";
import { useTranslation } from "react-i18next";
import { Checkbox } from "@/lib/components/Checkbox";
import type { SelectionFilter } from "@/types";

interface Props {
  currentFilter: SelectionFilter;
  open: boolean;
  onChange: (filter: SelectionFilter) => void;
  onClose: () => void;
}

type BooleanFilterKey = {
  [Key in keyof SelectionFilter]: SelectionFilter[Key] extends boolean
    ? Key
    : never;
}[keyof SelectionFilter];

interface FilterRow {
  key: BooleanFilterKey;
  labelKey: string;
}

const sketchGeometryRows: FilterRow[] = [
  { key: "select_curves", labelKey: "selectionFilter.curves" },
  { key: "select_points", labelKey: "selectionFilter.points" },
  {
    key: "select_construction",
    labelKey: "selectionFilter.constructionGeometry",
  },
  { key: "select_constraints", labelKey: "selectionFilter.constraints" },
];

const snapTypeRows: FilterRow[] = [
  { key: "snap_endpoint", labelKey: "selectionFilter.endpoint" },
  { key: "snap_midpoint", labelKey: "selectionFilter.midpoint" },
  { key: "snap_center", labelKey: "selectionFilter.center" },
  { key: "snap_intersection", labelKey: "selectionFilter.intersection" },
  { key: "snap_nearest", labelKey: "selectionFilter.nearest" },
  { key: "snap_circle_body", labelKey: "selectionFilter.circleBody" },
  { key: "snap_quadrant", labelKey: "selectionFilter.quadrant" },
  { key: "snap_perpendicular", labelKey: "selectionFilter.perpendicular" },
  { key: "snap_parallel", labelKey: "selectionFilter.parallel" },
  { key: "snap_tangent", labelKey: "selectionFilter.tangent" },
  { key: "snap_grid", labelKey: "selectionFilter.grid" },
  { key: "snap_polar", labelKey: "selectionFilter.polar" },
];

function CloseIcon() {
  return (
    <svg
      className="h-4 w-4"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.7"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M4 4l8 8M12 4l-8 8" />
    </svg>
  );
}

function PanelSection({
  title,
  children,
}: {
  title: string;
  children: ReactNode;
}) {
  return (
    <section className="border-t border-[var(--cad-panel-soft-border)] pt-4 first:border-t-0 first:pt-0">
      <p className="cad-kicker mb-3">{title}</p>
      {children}
    </section>
  );
}

export function SelectionFilterPanel({
  currentFilter,
  open,
  onChange,
  onClose,
}: Props) {
  const { t } = useTranslation();
  const [draft, setDraft] = useState<SelectionFilter>(currentFilter);

  useEffect(() => {
    setDraft(currentFilter);
  }, [currentFilter]);

  useEffect(() => {
    if (!open) {
      return undefined;
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (event.key !== "Escape") {
        return;
      }
      event.preventDefault();
      onClose();
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [onClose, open]);

  if (!open) return null;

  function toggle(key: BooleanFilterKey) {
    const next = { ...draft, [key]: !draft[key] };
    setDraft(next);
    onChange(next);
  }

  function handleTolerance(val: string) {
    const px = parseInt(val, 10);
    if (isNaN(px) || px <= 0) return;
    const next = { ...draft, tolerance_px: px };
    setDraft(next);
    onChange(next);
  }

  function handlePolarAngle(val: string) {
    const degrees = Number.parseInt(val, 10);
    if (Number.isNaN(degrees) || degrees <= 0 || degrees > 90) {
      return;
    }
    const next = { ...draft, polar_angle_degrees: degrees };
    setDraft(next);
    onChange(next);
  }

  function handleParallelAngle(val: string) {
    const degrees = Number.parseInt(val, 10);
    if (Number.isNaN(degrees) || degrees <= 0 || degrees > 45) {
      return;
    }
    const next = { ...draft, parallel_angle_degrees: degrees };
    setDraft(next);
    onChange(next);
  }

  function renderCheckboxRow(row: FilterRow) {
    return (
      <label
        key={row.key}
        className="flex cursor-pointer select-none items-center gap-2 rounded-lg px-2 py-1.5 text-sm text-on-surface transition-colors hover:bg-[var(--cad-subtle-surface)]"
      >
        <Checkbox
          checked={draft[row.key]}
          onCheckedChange={() => {
            toggle(row.key);
          }}
          ariaLabel={t(row.labelKey)}
        />
        <span>{t(row.labelKey)}</span>
      </label>
    );
  }

  return (
    <div className="selection-filter-panel cad-floating-panel w-[28rem] px-5 py-5">
      <div className="mb-5 flex items-start justify-between gap-4">
        <div>
          <p className="cad-kicker">{t("selectionFilter.kicker")}</p>
          <h3 className="mt-2 font-display text-lg tracking-[0.08em] text-on-surface">
            {t("selectionFilter.title")}
          </h3>
        </div>
        <button
          type="button"
          className="cad-ribbon-action h-8 w-8 px-0 py-0 text-on-surface-muted hover:text-on-surface"
          aria-label={t("common.close")}
          title={t("common.close")}
          onClick={onClose}
        >
          <CloseIcon />
        </button>
      </div>

      <div className="space-y-4">
        <PanelSection title={t("selectionFilter.sketchGeometry")}>
          <div className="grid gap-1">
            {sketchGeometryRows.map(renderCheckboxRow)}
          </div>
        </PanelSection>

        <PanelSection title={t("selectionFilter.snapTypes")}>
          <div className="grid grid-cols-2 gap-x-3 gap-y-1">
            {snapTypeRows.map(renderCheckboxRow)}
          </div>
          <label className="mt-3 flex items-center justify-between gap-4 rounded-xl px-2 py-2 text-sm text-on-surface">
            <span className="text-on-surface-muted">
              {t("selectionFilter.polarAngle")}
            </span>
            <span className="flex items-center gap-1">
              <input
                className="cad-input w-16 text-right text-sm"
                type="number"
                value={draft.polar_angle_degrees}
                onChange={(event) => handlePolarAngle(event.target.value)}
                min={5}
                max={90}
                step={5}
              />
              <span className="text-on-surface-muted">
                {t("selectionFilter.degreesShort")}
              </span>
            </span>
          </label>
          <label className="mt-3 flex items-center justify-between gap-4 rounded-xl px-2 py-2 text-sm text-on-surface">
            <span className="text-on-surface-muted">
              {t("selectionFilter.parallelAngle")}
            </span>
            <span className="flex items-center gap-1">
              <input
                className="cad-input w-16 text-right text-sm"
                type="number"
                value={draft.parallel_angle_degrees}
                onChange={(event) => handleParallelAngle(event.target.value)}
                min={1}
                max={45}
                step={1}
              />
              <span className="text-on-surface-muted">
                {t("selectionFilter.degreesShort")}
              </span>
            </span>
          </label>
        </PanelSection>

        <PanelSection title={t("selectionFilter.global")}>
          <div className="space-y-1">
            <label className="flex items-center justify-between gap-4 rounded-xl px-2 py-2 text-sm text-on-surface">
              <span className="text-on-surface-muted">
                {t("selectionFilter.tolerance")}
              </span>
              <input
                className="cad-input w-16 text-right text-sm"
                type="number"
                value={draft.tolerance_px}
                onChange={(event) => handleTolerance(event.target.value)}
                min={1}
                max={50}
              />
            </label>
            {renderCheckboxRow({
              key: "magnetic_pull",
              labelKey: "selectionFilter.magneticPull",
            })}
          </div>
        </PanelSection>
      </div>
    </div>
  );
}
