import { useTranslation } from "react-i18next";

export interface DrawingToolbarActions {
  /** Number of drawings in the document (Delete enabled when > 0). */
  drawingCount: number;
  /** Number of views on the active drawing (Insert enabled when > 0). */
  viewCount: number;
  onNewDrawing: () => void;
  onInsertView: () => void;
  onDeleteDrawing: () => void;
  /** Opens the sheet settings panel (paper, orientation, angle). */
  onSheetSettings: () => void;
  /** Arms the dimension tool (P6: pick an edge → live value → Enter). */
  onDimension: () => void;
  /** Exports the active sheet as SVG/DXF (P8). */
  onExportSvg: () => void;
  onExportDxf: () => void;
}

export interface DrawingToolbarProps extends Partial<DrawingToolbarActions> {
  disabled: boolean;
}

/** Toolbar for the ISO Drawing workspace: drawing lifecycle + view
 *  insertion + sheet settings + the dimension tool (P6) + sheet
 *  export (P8). */
export function DrawingToolbar({
  disabled,
  drawingCount = 0,
  viewCount = 0,
  onNewDrawing,
  onInsertView,
  onDeleteDrawing,
  onSheetSettings,
  onDimension,
  onExportSvg,
  onExportDxf,
}: DrawingToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="flex items-center gap-3">
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount > 0}
        title={t("drawing.toolbar.newDrawingTitle")}
        onClick={onNewDrawing}
      >
        + {t("drawing.toolbar.newDrawing")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount === 0}
        title={t("drawing.toolbar.insertViewTitle")}
        onClick={onInsertView}
      >
        {t("drawing.toolbar.insertView")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount === 0}
        title={t("drawing.toolbar.deleteDrawingTitle")}
        onClick={onDeleteDrawing}
      >
        {t("drawing.toolbar.deleteDrawing")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount === 0}
        title={t("drawing.toolbar.sheetSettingsTitle")}
        onClick={onSheetSettings}
      >
        {t("drawing.toolbar.sheetSettings")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || viewCount === 0}
        title={t("drawing.toolbar.dimensionTitle")}
        onClick={onDimension}
      >
        {t("drawing.toolbar.dimension")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount === 0}
        title={t("drawing.toolbar.exportSvgTitle")}
        onClick={onExportSvg}
      >
        {t("drawing.toolbar.exportSvg")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled || drawingCount === 0}
        title={t("drawing.toolbar.exportDxfTitle")}
        onClick={onExportDxf}
      >
        {t("drawing.toolbar.exportDxf")}
      </button>
      <span className="text-xs text-[var(--cad-muted)]">
        {drawingCount > 0
          ? t("drawing.toolbar.viewCount", { count: viewCount })
          : t("drawing.toolbar.noDrawing")}
      </span>
    </div>
  );
}
