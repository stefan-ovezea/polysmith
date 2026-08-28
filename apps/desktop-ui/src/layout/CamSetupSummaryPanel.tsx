import { useTranslation } from "react-i18next";
import type { DocumentState } from "../types";

interface CamSetupSummaryPanelProps {
  document: DocumentState | null;
  onOpenSetup: () => void;
}

// CAM sidebar section above the operation list: shows the active setup
// (machine + stock + units) and the tool library, so creating a setup
// through the Setup button has a visible home.  CAM entities live in
// the CAM workspace, not in the CAD feature tree.
export function CamSetupSummaryPanel({
  document,
  onOpenSetup,
}: CamSetupSummaryPanelProps) {
  const { t } = useTranslation();
  const setup = document?.cam.setups[0];
  const tools = document?.cam.tool_library ?? [];

  const stock = setup?.stock;
  const stockLabel =
    stock?.type === "bounding_box" && stock.size
      ? `${stock.size[0]} × ${stock.size[1]} × ${stock.size[2]} mm`
      : stock?.type === "cylinder" && stock.diameter && stock.length
        ? `Ø ${stock.diameter} × ${stock.length} mm`
        : t("cam.setupSummary.stockDefault");

  return (
    <div className="flex flex-col gap-2 border-b border-[var(--cad-panel-border)] px-3 py-2">
      <div className="flex items-center justify-between">
        <span className="text-xs font-semibold cad-muted tracking-wider uppercase">
          {t("cam.setupSummary.title")}
        </span>
        <button
          type="button"
          className="text-xs cad-muted hover:text-on-surface underline underline-offset-2"
          onClick={onOpenSetup}
        >
          {t("cam.setupSummary.edit")}
        </button>
      </div>
      {setup ? (
        <div className="flex flex-col gap-1 text-xs">
          <div className="flex items-center justify-between gap-2">
            <span className="cad-muted">{setup.name}</span>
            <span>
              {setup.machine_type === "laser"
                ? t("cam.setupSummary.machineLaser")
                : setup.machine_type === "plasma"
                  ? t("cam.setupSummary.machinePlasma")
                  : setup.machine_type === "printer"
                    ? t("cam.setupSummary.machinePrinter")
                    : setup.machine_type}
            </span>
          </div>
          <div className="flex items-center justify-between gap-2">
            <span className="cad-muted">{t("cam.setupSummary.stock")}</span>
            <span>{stockLabel}</span>
          </div>
          {tools.length > 0 ? (
            <div className="flex items-center justify-between gap-2">
              <span className="cad-muted">{t("cam.setupSummary.tools")}</span>
              <span>
                {tools.map((tool) => tool.name).join(", ")}
              </span>
            </div>
          ) : (
            <span className="cad-muted">{t("cam.setupSummary.noTools")}</span>
          )}
        </div>
      ) : (
        <p className="text-xs cad-muted">
          {t("cam.setupSummary.noSetup")}
        </p>
      )}
    </div>
  );
}
