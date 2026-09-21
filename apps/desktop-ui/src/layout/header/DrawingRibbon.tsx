// The Fusion-style drawing ribbon (R1): the per-tab tool rows of the
// drawing workspace.  The six tabs themselves live in AppHeader's
// existing sub-tab nav (drawingWorkspaces); this component renders
// the tools of the ACTIVE tab, the contextual toolbar strip while a
// tool is armed (the SketchToolbar project-strip precedent), and the
// export buttons.  ANNOTATE (Text / Leader Text), GEOMETRY (Center
// Mark / Centerline / Edge Extension) and SYMBOLS (Surface Finish /
// Welding / Tolerance Frame / Datum / Balloon) are live as of R5 —
// each arms the annotation panel with its kind.

import { useTranslation } from "react-i18next";

import { Dropdown } from "@/lib";
import { ISO_SCALES } from "@/lib/drawingViewMath";
import type {
  BaseOrientation,
  BaseViewSettings,
  DrawingTool,
} from "@/types";

export type DrawingTabId =
  | "views"
  | "geometry"
  | "dimension"
  | "symbols"
  | "annotate"
  | "modify";

interface ToolDef {
  id: string;
  labelKey: string;
  titleKey: string;
  enabled: boolean;
  active?: boolean;
  placeholder?: boolean;
  onSelect?: () => void;
}

export interface DrawingRibbonProps {
  disabled?: boolean;
  activeTab: DrawingTabId;
  drawingCount: number;
  viewCount: number;
  bodyCount: number;
  tool: DrawingTool;
  base: BaseViewSettings;
  projectedParentLabel: string | null;
  projectionAngle: "first_angle" | "third_angle";
  dimensionPanelOpen: boolean;
  /** The ANNOTATE → Text tool is armed (click-to-place a free note). */
  noteToolArmed: boolean;
  /** The armed annotation-panel kind ("leader_text" in P1; the
   *  GEOMETRY/SYMBOLS kinds join in P2/P3) — drives the active state. */
  annotationPanelKind: string | null;
  /** A CAD camera snapshot exists (the user oriented the 3D viewport)
   *  — gates the "Current 3D view" strip button. */
  cameraAvailable: boolean;
  availableBodies: Array<{ id: string; label: string }>;
  onNewDrawing: () => void;
  onBaseView: () => void;
  onProjectedView: () => void;
  onSection: () => void;
  onDetailView: () => void;
  onDeleteView: () => void;
  onDimension: () => void;
  onText: () => void;
  onLeaderText: () => void;
  onCenterMark: () => void;
  onCenterline: () => void;
  onEdgeExtension: () => void;
  onSurfaceFinish: () => void;
  onWelding: () => void;
  onToleranceFrame: () => void;
  onDatum: () => void;
  onBalloon: () => void;
  onMove: () => void;
  onSheetSettings: () => void;
  onDeleteDrawing: () => void;
  onExportSvg: () => void;
  onExportDxf: () => void;
  onExportDxfAnnotated: () => void;
  onExportPdf: () => void;
  onSetOrientation: (orientation: BaseOrientation) => void;
  onCaptureCurrent3d: () => void;
  onSetScale: (scale: number) => void;
  onSetShowHidden: (showHidden: boolean) => void;
  onSetBodyChoice: (bodyChoice: string) => void;
}

export function DrawingRibbon({
  disabled = false,
  activeTab,
  drawingCount,
  viewCount,
  bodyCount,
  tool,
  base,
  projectedParentLabel,
  projectionAngle,
  dimensionPanelOpen,
  noteToolArmed,
  annotationPanelKind,
  cameraAvailable,
  availableBodies,
  onNewDrawing,
  onBaseView,
  onProjectedView,
  onSection,
  onDetailView,
  onDeleteView,
  onDimension,
  onText,
  onLeaderText,
  onCenterMark,
  onCenterline,
  onEdgeExtension,
  onSurfaceFinish,
  onWelding,
  onToleranceFrame,
  onDatum,
  onBalloon,
  onMove,
  onSheetSettings,
  onDeleteDrawing,
  onExportSvg,
  onExportDxf,
  onExportDxfAnnotated,
  onExportPdf,
  onSetOrientation,
  onCaptureCurrent3d,
  onSetScale,
  onSetShowHidden,
  onSetBodyChoice,
}: DrawingRibbonProps) {
  const { t } = useTranslation();

  const hasDrawing = drawingCount > 0;
  const hasViews = viewCount > 0;
  const hasBodies = bodyCount > 0;

  const tools = buildToolsForTab(
    activeTab,
    { hasDrawing, hasViews, hasBodies },
    {
      tool,
      dimensionPanelOpen,
      noteToolArmed,
      annotationPanelKind,
      onNewDrawing,
      onBaseView,
      onProjectedView,
      onSection,
      onDetailView,
      onDeleteView,
      onDimension,
      onText,
      onLeaderText,
      onCenterMark,
      onCenterline,
      onEdgeExtension,
      onSurfaceFinish,
      onWelding,
      onToleranceFrame,
      onDatum,
      onBalloon,
      onMove,
      onSheetSettings,
      onDeleteDrawing,
    },
  );

  return (
    <div className="flex w-full items-center gap-1.5">
      {tools.map((toolDef) => (
        <button
          key={toolDef.id}
          type="button"
          className={
            toolDef.active
              ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-3 leading-none"
              : "cad-ribbon-action h-9 px-3 leading-none"
          }
          data-tooltip={
            toolDef.placeholder
              ? `${t(toolDef.titleKey)} — ${t("drawing.ribbon.nextPhaseHint")}`
              : t(toolDef.titleKey)
          }
          disabled={disabled || !toolDef.enabled}
          onClick={toolDef.onSelect}
        >
          {t(toolDef.labelKey)}
        </button>
      ))}
      <DrawingToolStrip
        disabled={disabled}
        tool={tool}
        base={base}
        projectedParentLabel={projectedParentLabel}
        projectionAngle={projectionAngle}
        cameraAvailable={cameraAvailable}
        availableBodies={availableBodies}
        bodyCount={bodyCount}
        onSetOrientation={onSetOrientation}
        onCaptureCurrent3d={onCaptureCurrent3d}
        onSetScale={onSetScale}
        onSetShowHidden={onSetShowHidden}
        onSetBodyChoice={onSetBodyChoice}
      />
      <div className="flex-1" />
      <div className="flex items-center gap-1.5">
        <button
          type="button"
          className="cad-ribbon-action h-9 px-3 leading-none"
          data-tooltip={t("drawing.toolbar.exportSvgTitle")}
          disabled={disabled || !hasDrawing}
          onClick={onExportSvg}
        >
          {t("drawing.toolbar.exportSvg")}
        </button>
        <button
          type="button"
          className="cad-ribbon-action h-9 px-3 leading-none"
          data-tooltip={t("drawing.toolbar.exportDxfTitle")}
          disabled={disabled || !hasDrawing}
          onClick={onExportDxf}
        >
          {t("drawing.toolbar.exportDxf")}
        </button>
        <button
          type="button"
          className="cad-ribbon-action h-9 px-3 leading-none"
          data-tooltip={t("drawing.toolbar.exportDxfAnnotatedTitle")}
          disabled={disabled || !hasDrawing}
          onClick={onExportDxfAnnotated}
        >
          {t("drawing.toolbar.exportDxfAnnotated")}
        </button>
        <button
          type="button"
          className="cad-ribbon-action h-9 px-3 leading-none"
          data-tooltip={t("drawing.toolbar.exportPdfTitle")}
          disabled={disabled || !hasDrawing}
          onClick={onExportPdf}
        >
          {t("drawing.toolbar.exportPdf")}
        </button>
      </div>
      <span className="px-1 text-xs opacity-60">
        {t("drawing.toolbar.viewCount", { count: viewCount })}
      </span>
    </div>
  );
}

// ── Tool tables ────────────────────────────────────────────────────

interface TabContext {
  hasDrawing: boolean;
  hasViews: boolean;
  hasBodies: boolean;
}

interface TabActions {
  tool: DrawingTool;
  dimensionPanelOpen: boolean;
  noteToolArmed: boolean;
  annotationPanelKind: string | null;
  onNewDrawing: () => void;
  onBaseView: () => void;
  onProjectedView: () => void;
  onSection: () => void;
  onDetailView: () => void;
  onDeleteView: () => void;
  onDimension: () => void;
  onText: () => void;
  onLeaderText: () => void;
  onCenterMark: () => void;
  onCenterline: () => void;
  onEdgeExtension: () => void;
  onSurfaceFinish: () => void;
  onWelding: () => void;
  onToleranceFrame: () => void;
  onDatum: () => void;
  onBalloon: () => void;
  onMove: () => void;
  onSheetSettings: () => void;
  onDeleteDrawing: () => void;
}

function buildToolsForTab(
  tab: DrawingTabId,
  context: TabContext,
  actions: TabActions,
): ToolDef[] {
  const { hasDrawing, hasViews, hasBodies } = context;
  switch (tab) {
    case "views":
      return [
        {
          id: "newDrawing",
          labelKey: "drawing.ribbon.tools.newDrawing",
          titleKey: "drawing.ribbon.tools.newDrawingTitle",
          enabled: !hasDrawing,
          onSelect: actions.onNewDrawing,
        },
        {
          id: "baseView",
          labelKey: "drawing.ribbon.tools.baseView",
          titleKey: "drawing.ribbon.tools.baseViewTitle",
          enabled: hasDrawing && hasBodies,
          active: actions.tool === "base_view",
          onSelect: actions.onBaseView,
        },
        {
          id: "projectedView",
          labelKey: "drawing.ribbon.tools.projectedView",
          titleKey: "drawing.ribbon.tools.projectedViewTitle",
          enabled: hasViews,
          active: actions.tool === "projected_view",
          onSelect: actions.onProjectedView,
        },
        {
          id: "section",
          labelKey: "drawing.ribbon.tools.section",
          titleKey: "drawing.ribbon.tools.sectionTitle",
          enabled: hasDrawing && hasBodies,
          active: actions.tool === "section",
          onSelect: actions.onSection,
        },
        {
          id: "detailView",
          labelKey: "drawing.ribbon.tools.detailView",
          titleKey: "drawing.ribbon.tools.detailViewTitle",
          enabled: hasViews,
          active: actions.tool === "detail_view",
          onSelect: actions.onDetailView,
        },
        {
          id: "deleteView",
          labelKey: "drawing.ribbon.tools.deleteView",
          titleKey: "drawing.ribbon.tools.deleteViewTitle",
          enabled: hasViews,
          active: actions.tool === "delete_view",
          onSelect: actions.onDeleteView,
        },
      ];
    case "geometry":
      return [
        {
          id: "centerMark",
          labelKey: "drawing.ribbon.tools.centerMark",
          titleKey: "drawing.ribbon.tools.centerMarkTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "center_mark",
          onSelect: actions.onCenterMark,
        },
        {
          id: "centerline",
          labelKey: "drawing.ribbon.tools.centerline",
          titleKey: "drawing.ribbon.tools.centerlineTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "centerline",
          onSelect: actions.onCenterline,
        },
        {
          id: "edgeExtension",
          labelKey: "drawing.ribbon.tools.edgeExtension",
          titleKey: "drawing.ribbon.tools.edgeExtensionTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "edge_extension",
          onSelect: actions.onEdgeExtension,
        },
      ];
    case "dimension":
      return [
        {
          id: "dimension",
          labelKey: "drawing.ribbon.tools.dimension",
          titleKey: "drawing.ribbon.tools.dimensionTitle",
          enabled: hasViews,
          active: actions.dimensionPanelOpen,
          onSelect: actions.onDimension,
        },
      ];
    case "symbols":
      return [
        {
          id: "surfaceFinish",
          labelKey: "drawing.ribbon.tools.surfaceFinish",
          titleKey: "drawing.ribbon.tools.surfaceFinishTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "surface_finish",
          onSelect: actions.onSurfaceFinish,
        },
        {
          id: "welding",
          labelKey: "drawing.ribbon.tools.welding",
          titleKey: "drawing.ribbon.tools.weldingTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "welding",
          onSelect: actions.onWelding,
        },
        {
          id: "toleranceFrame",
          labelKey: "drawing.ribbon.tools.toleranceFrame",
          titleKey: "drawing.ribbon.tools.toleranceFrameTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "tolerance_frame",
          onSelect: actions.onToleranceFrame,
        },
        {
          id: "datum",
          labelKey: "drawing.ribbon.tools.datum",
          titleKey: "drawing.ribbon.tools.datumTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "datum",
          onSelect: actions.onDatum,
        },
        {
          id: "balloon",
          labelKey: "drawing.ribbon.tools.balloon",
          titleKey: "drawing.ribbon.tools.balloonTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "balloon",
          onSelect: actions.onBalloon,
        },
      ];
    case "annotate":
      return [
        {
          id: "text",
          labelKey: "drawing.ribbon.tools.text",
          titleKey: "drawing.ribbon.tools.textTitle",
          enabled: hasDrawing,
          active: actions.noteToolArmed,
          onSelect: actions.onText,
        },
        {
          id: "leaderText",
          labelKey: "drawing.ribbon.tools.leaderText",
          titleKey: "drawing.ribbon.tools.leaderTextTitle",
          enabled: hasViews,
          active: actions.annotationPanelKind === "leader_text",
          onSelect: actions.onLeaderText,
        },
      ];
    case "modify":
      return [
        {
          id: "move",
          labelKey: "drawing.ribbon.tools.move",
          titleKey: "drawing.ribbon.tools.moveTitle",
          enabled: hasViews,
          active: actions.tool === "move",
          onSelect: actions.onMove,
        },
        {
          id: "delete",
          labelKey: "drawing.ribbon.tools.delete",
          titleKey: "drawing.ribbon.tools.deleteTitle",
          enabled: hasViews,
          active: actions.tool === "delete_view",
          onSelect: actions.onDeleteView,
        },
        {
          id: "sheetSettings",
          labelKey: "drawing.ribbon.tools.sheetSettings",
          titleKey: "drawing.ribbon.tools.sheetSettingsTitle",
          enabled: hasDrawing,
          onSelect: actions.onSheetSettings,
        },
        {
          id: "deleteDrawing",
          labelKey: "drawing.ribbon.tools.deleteDrawing",
          titleKey: "drawing.ribbon.tools.deleteDrawingTitle",
          enabled: hasDrawing,
          onSelect: actions.onDeleteDrawing,
        },
      ];
  }
}

// ── The contextual strip ───────────────────────────────────────────

interface DrawingToolStripProps {
  disabled: boolean;
  tool: DrawingTool;
  base: BaseViewSettings;
  projectedParentLabel: string | null;
  projectionAngle: "first_angle" | "third_angle";
  cameraAvailable: boolean;
  availableBodies: Array<{ id: string; label: string }>;
  bodyCount: number;
  onSetOrientation: (orientation: BaseOrientation) => void;
  onCaptureCurrent3d: () => void;
  onSetScale: (scale: number) => void;
  onSetShowHidden: (showHidden: boolean) => void;
  onSetBodyChoice: (bodyChoice: string) => void;
}

function DrawingToolStrip({
  disabled,
  tool,
  base,
  projectedParentLabel,
  projectionAngle,
  cameraAvailable,
  availableBodies,
  bodyCount,
  onSetOrientation,
  onCaptureCurrent3d,
  onSetScale,
  onSetShowHidden,
  onSetBodyChoice,
}: DrawingToolStripProps) {
  const { t } = useTranslation();

  if (tool === "idle") {
    return null;
  }

  return (
    <>
      <div className="h-8 w-px bg-white/10" />
      {tool === "base_view" ? (
        <>
          <span className="px-1 text-xs opacity-60">
            {t("drawing.strip.orientation")}
          </span>
          {(["front", "right", "left", "top", "bottom", "back"] as const).map(
            (name) => (
              <button
                key={name}
                type="button"
                className={
                  base.orientation === name
                    ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-2 leading-none"
                    : "cad-ribbon-action h-9 px-2 leading-none"
                }
                disabled={disabled}
                onClick={() => onSetOrientation(name)}
              >
                {t(`drawing.insertPanel.${name}`)}
              </button>
            ),
          )}
          {(["ne", "nw", "se", "sw"] as const).map((name) => (
            <button
              key={name}
              type="button"
              className={
                base.orientation === name
                  ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-2 leading-none"
                  : "cad-ribbon-action h-9 px-2 leading-none"
              }
              data-tooltip={t(`drawing.strip.${name}`)}
              disabled={disabled}
              onClick={() => onSetOrientation(name)}
            >
              {t(`drawing.strip.${name}`)}
            </button>
          ))}
          <button
            type="button"
            className={
              base.orientation === "current3d"
                ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-3 leading-none"
                : "cad-ribbon-action h-9 px-3 leading-none"
            }
            data-tooltip={
              cameraAvailable
                ? t("drawing.strip.current3d")
                : t("drawing.strip.current3dNoCamera")
            }
            disabled={disabled || !cameraAvailable}
            onClick={onCaptureCurrent3d}
          >
            {t("drawing.strip.current3d")}
          </button>
          <div className="h-8 w-px bg-white/10" />
          <Dropdown
            label={t("drawing.strip.scale")}
            className="w-[86px]"
            buttonClassName="h-9"
            value={String(base.scale)}
            options={ISO_SCALES.map((scale) => ({
              value: scale,
              label: scale,
            }))}
            onChange={(scale) => onSetScale(Number(scale) || 1)}
          />
          <label className="flex items-center gap-1.5 px-1 text-xs opacity-80">
            <input
              type="checkbox"
              checked={base.showHidden}
              disabled={disabled}
              onChange={(event) => onSetShowHidden(event.target.checked)}
            />
            {t("drawing.strip.hiddenLines")}
          </label>
          <Dropdown
            label={t("drawing.strip.body")}
            className="w-[150px]"
            buttonClassName="h-9"
            value={base.bodyChoice}
            options={[
              {
                value: "__all__",
                label: t("drawing.strip.bodyAll", { count: bodyCount }),
              },
              ...availableBodies.map((body) => ({
                value: body.id,
                label: body.label,
              })),
            ]}
            onChange={(bodyChoice) => onSetBodyChoice(bodyChoice)}
          />
        </>
      ) : null}
      {tool === "projected_view" ? (
        <>
          <span className="px-1 text-xs opacity-80">
            {projectedParentLabel
              ? t("drawing.strip.projectedParent", {
                  label: projectedParentLabel,
                })
              : t("drawing.strip.projectedNoParent")}
          </span>
          <span className="cad-ribbon-action h-9 cursor-default px-2 leading-none opacity-70">
            {projectionAngle === "first_angle"
              ? t("drawing.strip.firstAngle")
              : t("drawing.strip.thirdAngle")}
          </span>
        </>
      ) : null}
      <span className="px-1 text-xs opacity-60">
        {tool === "base_view"
          ? t("drawing.strip.baseHint")
          : tool === "projected_view"
            ? t("drawing.strip.projectedHint")
            : tool === "section"
              ? t("drawing.strip.sectionHint")
              : tool === "detail_view"
                ? t("drawing.strip.detailHint")
                : tool === "move"
                  ? t("drawing.strip.moveHint")
                  : t("drawing.strip.deleteHint")}
      </span>
    </>
  );
}
