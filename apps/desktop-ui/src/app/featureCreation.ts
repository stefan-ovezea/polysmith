import { awaitDocumentChange } from "../state";
import type { DocumentState } from "../types/ipc";

type FeatureEntry = DocumentState["feature_history"][number];

export type CreatedFeaturePredicate = (feature: FeatureEntry) => boolean;

export interface CreatedFeatureResult {
  document: DocumentState;
  feature: FeatureEntry;
  featureId: string;
  createdFeatures: FeatureEntry[];
}

export async function awaitCreatedFeature(
  predicate: CreatedFeaturePredicate,
): Promise<CreatedFeatureResult> {
  let result: CreatedFeatureResult | null = null;
  const document = await awaitDocumentChange((next, previous) => {
    const selectedFeatureId = next.selected_feature_id;
    if (!selectedFeatureId) {
      return false;
    }

    const previousLength = previous?.feature_history.length ?? 0;
    if (next.feature_history.length <= previousLength) {
      return false;
    }

    const createdFeatures = next.feature_history
      .slice(previousLength)
      .filter(predicate);
    const selectedFeature = createdFeatures.find(
      (feature) => feature.feature_id === selectedFeatureId,
    );
    if (!selectedFeature) {
      return false;
    }

    result = {
      document: next,
      feature: selectedFeature,
      featureId: selectedFeatureId,
      createdFeatures,
    };
    return true;
  });

  if (result) {
    return { ...result, document };
  }

  throw new Error("awaitCreatedFeature: matched document without feature");
}

export function awaitCreatedFeatureOfKind(kind: string) {
  return awaitCreatedFeature((feature) => feature.kind === kind);
}
