import { ToolbarTooltip } from "@/lib";
import type {
  ArmedSketchConstraint,
  DocumentState,
  ViewportState,
} from "@/types";
import type { SelectedConstraintState } from "./contextMenuState";
import { GridMiniIcon } from "./draftDimensions";
import type { ConstraintPreviewState } from "./constraintPreview";
import type { SelectionRectOverlay } from "./selectionGeometry";

type ViewportTranslate = (
  key: string,
  options?: Record<string, unknown>,
) => string;

interface GridToggleToolbarProps {
  active: boolean;
  label: string;
  onToggle: () => void;
}

export function GridToggleToolbar({
  active,
  label,
  onToggle,
}: GridToggleToolbarProps) {
  return (
    <div className="pointer-events-auto absolute bottom-4 left-1/2 z-20 -translate-x-1/2">
      <div className="cad-view-mini-toolbar flex items-center gap-1 px-1.5 py-1.5 backdrop-blur-xl">
        <ToolbarTooltip label={label}>
          <button
            type="button"
            className={
              active
                ? "cad-view-mini-button cad-view-mini-button-active"
                : "cad-view-mini-button"
            }
            aria-label={label}
            aria-pressed={active}
            onClick={onToggle}
          >
            <GridMiniIcon />
          </button>
        </ToolbarTooltip>
      </div>
    </div>
  );
}

interface CrosshairGuideOverlayProps {
  pointer: { x: number; y: number } | null;
  size: number;
  visible: boolean;
}

export function CrosshairGuideOverlay({
  pointer,
  size,
  visible,
}: CrosshairGuideOverlayProps) {
  if (!visible || !pointer || size <= 0) {
    return null;
  }

  return (
    <div
      className="cad-crosshair-guide"
      style={{
        left: pointer.x,
        top: pointer.y,
        width: size,
        height: size,
        transform: "translate(-50%, -50%)",
      }}
    />
  );
}

export function SelectionRectangleOverlay({
  overlay,
}: {
  overlay: SelectionRectOverlay | null;
}) {
  if (!overlay?.visible) {
    return null;
  }

  return (
    <div
      className="pointer-events-none fixed z-30"
      style={{
        left: `${overlay.left}px`,
        top: `${overlay.top}px`,
        width: `${overlay.width}px`,
        height: `${overlay.height}px`,
        border:
          overlay.direction === "window"
            ? "1px solid var(--color-primary-edge-active, #4fc3f7)"
            : "1px dashed var(--color-destructive, #4caf50)",
        background:
          overlay.direction === "window"
            ? "rgba(79, 195, 247, 0.07)"
            : "rgba(76, 175, 80, 0.07)",
      }}
    />
  );
}

export function ConstraintPreviewBadge({
  preview,
}: {
  preview: ConstraintPreviewState | null;
}) {
  if (!preview) {
    return null;
  }

  return (
    <div
      className="pointer-events-none absolute z-30 flex h-5 w-5 items-center justify-center rounded-full border border-cyan-300/70 bg-slate-900/85 text-[10px] font-semibold text-cyan-200 shadow-md"
      style={{
        left: `${preview.x + 12}px`,
        top: `${preview.y + 12}px`,
      }}
    >
      {constraintPreviewGlyph(preview.kind)}
    </div>
  );
}

function constraintPreviewGlyph(kind: ConstraintPreviewState["kind"]) {
  switch (kind) {
    case "midpoint":
      return "M";
    case "perpendicular":
      return "\u22a5";
    case "horizontal":
      return "H";
    case "vertical":
      return "V";
    case "tangent":
      return "T";
    case "endpoint":
      return "\u25cf";
    case "parallel":
      return "\u2225";
    case "on_line":
      return "/";
  }
}

interface DimensionToolHintProps {
  hotkeyLabel: string;
  placeLabel: string;
  readyLabel: string;
  title: string;
  waitingForFirstLine: boolean;
  visible: boolean;
}

export function DimensionToolHint({
  hotkeyLabel,
  placeLabel,
  readyLabel,
  title,
  waitingForFirstLine,
  visible,
}: DimensionToolHintProps) {
  if (!visible) {
    return null;
  }

  return (
    <div className="cad-floating-panel pointer-events-auto absolute left-4 top-4 z-20 flex flex-col gap-1 px-3 py-2 text-xs">
      <p className="text-[10px] uppercase tracking-[0.18em] text-on-surface-dim">
        {title} <span className="opacity-60">({hotkeyLabel})</span>
      </p>
      <p className="text-on-surface">
        {waitingForFirstLine ? placeLabel : readyLabel}
      </p>
    </div>
  );
}

interface ViewportBlockingOverlayProps {
  kicker: string;
  message: string;
  variant: "empty" | "starting";
  visible: boolean;
}

export function ViewportBlockingOverlay({
  kicker,
  message,
  variant,
  visible,
}: ViewportBlockingOverlayProps) {
  if (!visible) {
    return null;
  }

  if (variant === "starting") {
    return (
      <div
        className="pointer-events-none absolute inset-0 flex items-center justify-center backdrop-blur-sm"
        style={{ background: "var(--cad-overlay-soft)" }}
      >
        <div className="cad-floating-panel flex min-w-[220px] items-center gap-4 px-5 py-4">
          <span className="cad-loader-spinner" aria-hidden="true" />
          <div>
            <p className="cad-kicker">{kicker}</p>
            <p className="mt-2 text-sm text-on-surface-muted">{message}</p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div
      className="absolute inset-0 flex items-center justify-center backdrop-blur-sm"
      style={{ background: "var(--cad-overlay-strong)" }}
    >
      <div className="text-center">
        <p className="cad-kicker">{kicker}</p>
        <p className="mt-4 text-sm text-on-surface-muted">{message}</p>
      </div>
    </div>
  );
}

interface ViewportStatusPanelProps {
  activeSketchPlaneId: string | null;
  activeSketchTool: string | null;
  arcCount: number;
  armedSketchConstraint: ArmedSketchConstraint;
  circleCount: number;
  document: DocumentState | null;
  finishDisabled: boolean;
  lineCount: number;
  lineDraftActive: boolean;
  measurementText: string | null;
  onFinishSketch: () => void;
  pointCount: number;
  selectedConstraint: SelectedConstraintState | null;
  selectedEntityDof: ViewportState["dof_statuses"][number] | null;
  selectedPrimitiveLabel: string | null;
  selectedReference: { label: string } | null;
  sketchSnapLabel: string | null;
  translate: ViewportTranslate;
  visible: boolean;
}

export function ViewportStatusPanel({
  activeSketchPlaneId,
  activeSketchTool,
  arcCount,
  armedSketchConstraint,
  circleCount,
  document,
  finishDisabled,
  lineCount,
  lineDraftActive,
  measurementText,
  onFinishSketch,
  pointCount,
  selectedConstraint,
  selectedEntityDof,
  selectedPrimitiveLabel,
  selectedReference,
  sketchSnapLabel,
  translate,
  visible,
}: ViewportStatusPanelProps) {
  if (!visible) {
    return null;
  }

  return (
    <div className="pointer-events-none absolute bottom-4 right-4 cad-floating-panel px-4 py-3 text-right">
      <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
        {translate("common.selection")}
      </p>
      <p className="mt-1 text-sm text-on-surface-muted">
        {selectedReference?.label ??
          selectedPrimitiveLabel ??
          translate("viewport.noSelection")}
      </p>
      {measurementText ? (
        <p className="mt-1 text-sm text-primary-soft">{measurementText}</p>
      ) : null}
      <p className="mt-1 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
        {activeSketchPlaneId
          ? translate("viewport.sketchStatus", {
              tool: activeSketchTool,
              lineCount,
              linePlural: lineCount === 1 ? "" : "s",
              circleCount,
              circlePlural: circleCount === 1 ? "" : "s",
              pointCount,
              pointPlural: pointCount === 1 ? "" : "s",
              arcCount,
              arcPlural: arcCount === 1 ? "" : "s",
            })
          : translate("viewport.noActiveSketch")}
      </p>
      {activeSketchPlaneId ? (
        <>
          <p className="mt-1 text-xs text-on-surface-dim">
            {sketchSelectionStatusText({
              activeSketchTool,
              armedSketchConstraint,
              document,
              lineDraftActive,
              selectedConstraint,
              selectedEntityDof,
              sketchSnapLabel,
              translate,
            })}
          </p>
          <button
            type="button"
            className="pointer-events-auto mt-3 ml-auto flex cad-ribbon-action cad-ribbon-action-primary"
            disabled={finishDisabled}
            onClick={onFinishSketch}
          >
            {translate("toolbar.finishSketch")}
          </button>
        </>
      ) : null}
    </div>
  );
}

function sketchSelectionStatusText({
  activeSketchTool,
  armedSketchConstraint,
  document,
  lineDraftActive,
  selectedConstraint,
  selectedEntityDof,
  sketchSnapLabel,
  translate,
}: Pick<
  ViewportStatusPanelProps,
  | "activeSketchTool"
  | "armedSketchConstraint"
  | "document"
  | "lineDraftActive"
  | "selectedConstraint"
  | "selectedEntityDof"
  | "sketchSnapLabel"
  | "translate"
>) {
  if (armedSketchConstraint) {
    return armedSketchConstraintStatusText(armedSketchConstraint, translate);
  }
  if (document?.selected_sketch_entity_id) {
    if (document.selected_sketch_dimension_id) {
      return translate("viewport.dimensionSelected");
    }
    if (selectedEntityDof) {
      return translate("viewport.entitySelectedDof", {
        entity: selectedEntityDof.entity_kind,
        dof: selectedEntityDof.total_dof,
        consumed: selectedEntityDof.consumed_dof,
        status:
          selectedEntityDof.status === "over"
            ? translate("viewport.dofOver")
            : selectedEntityDof.status === "full"
              ? translate("viewport.dofFull")
              : "",
      });
    }
    return translate("viewport.entitySelected");
  }
  if (sketchSnapLabel) {
    return `Snap: ${sketchSnapLabel}`;
  }
  if (document?.selected_sketch_vertex_id) {
    return translate("viewport.pointSelected");
  }
  if (document?.selected_sketch_profile_id) {
    return translate("viewport.profileSelected");
  }
  if (selectedConstraint) {
    return translate("viewport.constraintSelected", {
      kind: selectedConstraint.kind,
    });
  }
  if (activeSketchTool === "select") {
    return translate("viewport.selectionMode");
  }
  if (activeSketchTool === "project") {
    return translate("viewport.projectPrompt");
  }
  if (activeSketchTool === "line" && lineDraftActive) {
    return translate("viewport.lineChainActive");
  }
  return translate("viewport.clickPlaceGeometry");
}

function armedSketchConstraintStatusText(
  armedSketchConstraint: NonNullable<ArmedSketchConstraint>,
  translate: ViewportTranslate,
) {
  if (armedSketchConstraint.kind === "coincident") {
    return armedSketchConstraint.firstPointId
      ? translate("constraints.coincidentSecondPoint")
      : translate("constraints.coincidentFirstPoint");
  }
  if (
    armedSketchConstraint.kind === "equal_length" ||
    armedSketchConstraint.kind === "perpendicular" ||
    armedSketchConstraint.kind === "parallel"
  ) {
    return translate(
      armedSketchConstraint.firstLineId
        ? "constraints.lineSecond"
        : "constraints.lineFirst",
      {
        label: lineConstraintLabel(armedSketchConstraint.kind, translate),
      },
    );
  }
  return translate("constraints.lineConstraint", {
    kind: armedSketchConstraint.kind,
  });
}

function lineConstraintLabel(
  kind: "equal_length" | "perpendicular" | "parallel",
  translate: ViewportTranslate,
) {
  if (kind === "equal_length") {
    return translate("toolbar.equalLength");
  }
  if (kind === "perpendicular") {
    return translate("toolbar.perpendicular");
  }
  return translate("toolbar.parallel");
}
