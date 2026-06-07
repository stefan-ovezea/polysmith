import type { DocumentState } from "@/types";
import { useTranslation } from "react-i18next";

interface DocumentPanelProps {
  document: DocumentState | null;
  onSelectFeature: (featureId: string) => Promise<void>;
  onClearSelection: () => Promise<void>;
}

function DocumentPanel({
  document,
  onSelectFeature,
  onClearSelection,
}: DocumentPanelProps) {
  const { t } = useTranslation();
  if (!document) {
    return (
      <section className="flex h-full flex-col overflow-hidden px-4 py-4">
        <p className="cad-kicker">{t("common.browser")}</p>
        <h2 className="cad-title mt-2">{t("document.document")}</h2>
        <p className="mt-4 text-sm text-on-surface-muted">
          {t("document.noActiveDocument")}
        </p>
      </section>
    );
  }

  return (
    <section className="flex h-full min-h-0 flex-col overflow-hidden px-4 py-4">
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="cad-kicker">{t("common.browser")}</p>
          <h2 className="cad-title mt-2">{document.name}</h2>
        </div>
        <p className="text-xs uppercase tracking-[0.22em] text-on-surface-dim">
          {t("document.revision", { revision: document.revision })}
        </p>
      </div>

      <div className="mt-5 grid grid-cols-2 gap-2">
        <div className="cad-subtle-block rounded-2xl px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            {t("document.units")}
          </p>
          <p className="cad-metric mt-2">{document.units}</p>
        </div>
        <div className="cad-subtle-block rounded-2xl px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            {t("document.features")}
          </p>
          <p className="cad-metric mt-2">{document.feature_history.length}</p>
        </div>
      </div>

      <button
        className="cad-action-ghost mt-5 w-full justify-center"
        onClick={() => {
          void onClearSelection();
        }}
        disabled={
          document.selected_feature_id === null &&
          document.selected_reference_id === null
        }
      >
        {t("document.clearSelection")}
      </button>

      <div className="mt-4 space-y-3">
        <div className="cad-subtle-block rounded-2xl px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            {t("common.selection")}
          </p>
          <p className="mt-2 text-sm text-on-surface-muted">
            {document.selected_feature_id
              ? t("document.featureSelected")
              : document.selected_reference_id
                ? t("document.referenceSelected")
                : t("document.noSelection")}
          </p>
        </div>
        <div className="cad-subtle-block rounded-2xl px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            {t("document.sketchPlane")}
          </p>
          <p className="mt-2 text-sm text-on-surface-muted">
            {document.active_sketch_plane_id ?? t("document.activeSketchPlane")}
          </p>
        </div>
      </div>

      <div className="mt-5 flex items-center justify-between gap-3">
        <p className="font-display text-sm uppercase tracking-[0.16em] text-on-surface-muted">
          {t("document.hierarchy")}
        </p>
        <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
          {t("document.featureHistory")}
        </p>
      </div>

      <ul className="cad-scrollbar mt-3 flex-1 space-y-1 overflow-y-auto pr-1">
        {document.feature_history.map((feature) => (
          <li key={feature.feature_id}>
            <button
              className={`w-full rounded-2xl px-3 py-3 text-left transition-all duration-300 ${
                document.selected_feature_id === feature.feature_id
                  ? "cad-hierarchy-item-active"
                  : "cad-hierarchy-item"
              }`}
              onClick={() => {
                void onSelectFeature(feature.feature_id);
              }}
            >
              <div className="flex items-start justify-between gap-3">
                <div>
                  <p className="font-display text-sm tracking-[0.08em] text-on-surface">
                    {feature.name}
                  </p>
                  <p className="mt-1 text-xs uppercase tracking-[0.16em] text-on-surface-dim">
                    {feature.kind} · {feature.status}
                  </p>
                </div>
                {document.selected_feature_id === feature.feature_id ? (
                  <span className="cad-hierarchy-status-dot h-2.5 w-2.5 rounded-full bg-primary-glow" />
                ) : null}
              </div>
              <p className="mt-3 text-sm text-on-surface-muted">
                {feature.parameters_summary}
              </p>
            </button>
          </li>
        ))}
      </ul>
    </section>
  );
}
