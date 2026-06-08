import type { DocumentState } from "../types";

export interface SketchSourceLabels {
  sketchProfileLabelById: Map<string, string>;
  sketchLineLabelById: Map<string, string>;
  sketchPathEntityLabelById: Map<string, string>;
}

export function buildSketchSourceLabels(
  document: DocumentState | null,
): SketchSourceLabels {
  const sketchProfileLabelById = new Map<string, string>();
  const sketchLineLabelById = new Map<string, string>();
  const sketchPathEntityLabelById = new Map<string, string>();

  for (const feature of document?.feature_history ?? []) {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      continue;
    }
    feature.sketch_parameters.profiles.forEach((profile, index) => {
      sketchProfileLabelById.set(
        profile.profile_id,
        `${feature.name || "Sketch"} · Profile ${index + 1}`,
      );
    });
    feature.sketch_parameters.lines.forEach((line, index) => {
      const label = `${feature.name || "Sketch"} · Line ${index + 1}`;
      sketchLineLabelById.set(line.line_id, label);
      sketchPathEntityLabelById.set(line.line_id, label);
    });
    feature.sketch_parameters.arcs.forEach((arc, index) => {
      sketchPathEntityLabelById.set(
        arc.arc_id,
        `${feature.name || "Sketch"} · Arc ${index + 1}`,
      );
    });
  }

  return {
    sketchProfileLabelById,
    sketchLineLabelById,
    sketchPathEntityLabelById,
  };
}
