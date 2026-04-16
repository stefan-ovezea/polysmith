import type { DocumentState } from "../types/ipc";

interface FeatureTimelineProps {
  document: DocumentState | null;
  onSelectFeature: (featureId: string) => Promise<void>;
}

export function FeatureTimeline({
  document,
  onSelectFeature,
}: FeatureTimelineProps) {
  if (!document) {
    return null;
  }

  return (
    <div className="cad-panel pointer-events-auto absolute bottom-6 left-1/2 z-20 w-[min(880px,calc(100vw-28rem))] -translate-x-1/2 px-6 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">Feature Timeline</p>
        <p className="text-xs uppercase tracking-[0.22em] text-on-surface-dim">
          Revision {document.revision}
        </p>
      </div>

      <div className="mt-4 flex items-center gap-3 overflow-x-auto cad-scrollbar pb-2">
        {document.feature_history.map((feature, index) => {
          const active = feature.feature_id === document.selected_feature_id;
          return (
            <div key={feature.feature_id} className="flex min-w-max items-center gap-3">
              <button
                onClick={() => {
                  void onSelectFeature(feature.feature_id);
                }}
                className="group flex flex-col items-center gap-2"
              >
                <span
                  className="h-3 w-3 rounded-full transition-all duration-300"
                  style={{
                    backgroundColor: active ? "#00e5ff" : "rgba(229, 226, 225, 0.24)",
                    boxShadow: active
                      ? "0 0 16px rgba(0, 229, 255, 0.4)"
                      : "none",
                    transform: active ? "scale(1.25)" : undefined,
                  }}
                />
                <span
                  className={`text-xs tracking-[0.08em] ${
                    active ? "text-primary-soft" : "text-on-surface-dim"
                  }`}
                >
                  {feature.name}
                </span>
              </button>
              {index < document.feature_history.length - 1 ? (
                <div className="h-[2px] w-12 rounded-full bg-surface-high" />
              ) : null}
            </div>
          );
        })}
      </div>
    </div>
  );
}
