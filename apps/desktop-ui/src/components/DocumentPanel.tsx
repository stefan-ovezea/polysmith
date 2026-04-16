import type { DocumentState } from "../types/ipc";

interface DocumentPanelProps {
  document: DocumentState | null;
  onSelectFeature: (featureId: string) => Promise<void>;
  onClearSelection: () => Promise<void>;
}

export function DocumentPanel({
  document,
  onSelectFeature,
  onClearSelection,
}: DocumentPanelProps) {
  if (!document) {
    return (
      <section className="cad-panel h-full px-5 py-5">
        <p className="cad-kicker">Model Browser</p>
        <h2 className="cad-title mt-2">Document</h2>
        <p className="mt-4 text-sm text-on-surface-muted">No active document.</p>
      </section>
    );
  }

  return (
    <section className="cad-panel flex h-full flex-col px-5 py-5">
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="cad-kicker">Model Browser</p>
          <h2 className="cad-title mt-2">{document.name}</h2>
        </div>
        <p className="text-xs uppercase tracking-[0.22em] text-on-surface-dim">
          Rev {document.revision}
        </p>
      </div>

      <div className="mt-5 grid grid-cols-2 gap-3">
        <div className="cad-panel-soft px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            Units
          </p>
          <p className="cad-metric mt-2">{document.units}</p>
        </div>
        <div className="cad-panel-soft px-3 py-3">
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            Features
          </p>
          <p className="cad-metric mt-2">{document.feature_history.length}</p>
        </div>
      </div>

      <button
        className="cad-action-ghost mt-5 w-full"
        onClick={() => {
          void onClearSelection();
        }}
        disabled={document.selected_feature_id === null}
      >
        Clear Selection
      </button>

      <ul className="cad-scrollbar mt-5 flex-1 space-y-2 overflow-y-auto pr-1">
        {document.feature_history.map((feature) => (
          <li key={feature.feature_id}>
            <button
              className="cad-panel-soft w-full px-3 py-3 text-left transition-all duration-300"
              onClick={() => {
                void onSelectFeature(feature.feature_id);
              }}
              style={{
                boxShadow:
                  document.selected_feature_id === feature.feature_id
                    ? "inset 0 0 0 1px rgba(0, 229, 255, 0.55), 0 0 18px rgba(0, 229, 255, 0.12)"
                    : undefined,
                background:
                  document.selected_feature_id === feature.feature_id
                    ? "rgba(17, 80, 90, 0.55)"
                    : undefined,
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
                  <span className="h-2.5 w-2.5 rounded-full bg-primary-glow shadow-[0_0_14px_rgba(0,229,255,0.4)]" />
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
