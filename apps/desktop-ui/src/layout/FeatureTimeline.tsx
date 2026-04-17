import type { DocumentState } from "@/types";

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
    <div className="cad-timeline pointer-events-auto px-4 py-2.5">
      <div className="cad-scrollbar flex items-center gap-2 overflow-x-auto pb-1">
        {document.feature_history.map((feature, index) => {
          const active = feature.feature_id === document.selected_feature_id;
          return (
            <div
              key={feature.feature_id}
              className="flex min-w-max items-center gap-2"
            >
              <button
                onClick={() => {
                  void onSelectFeature(feature.feature_id);
                }}
                className="group flex flex-col items-center gap-1.5"
              >
                <span
                  className={`h-3 w-3 rounded-full transition-all duration-300 ${
                    active ? "cad-timeline-node-active" : "cad-timeline-node"
                  }`}
                />
                <span
                  className={`text-[11px] tracking-[0.06em] ${
                    active ? "text-primary-soft" : "text-on-surface-dim"
                  }`}
                >
                  {feature.name}
                </span>
              </button>
              {index < document.feature_history.length - 1 ? (
                <div className="h-[2px] w-8 rounded-full bg-surface-high" />
              ) : null}
            </div>
          );
        })}
      </div>
    </div>
  );
}
