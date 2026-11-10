import { describe, expect, it } from "vitest";

import {
  blockEndingAtFeatureCount,
  featureCountBeforeBlock,
  featureTimelineBlocks,
  type TimelineFeature,
} from "./featureTimelineBlocks";

function feature(id: string, operationId = ""): TimelineFeature {
  return {
    feature_id: id,
    kind: "extrude",
    name: "Extrude",
    status: "healthy",
    parameters_summary: "",
    operation_id: operationId,
  } as unknown as TimelineFeature;
}

describe("featureTimelineBlocks", () => {
  it("keeps standalone features as one row each", () => {
    const blocks = featureTimelineBlocks([
      feature("feature-1"),
      feature("feature-2"),
      feature("feature-3"),
    ]);
    expect(blocks.map((block) => block.primary.feature_id)).toEqual([
      "feature-1",
      "feature-2",
      "feature-3",
    ]);
    expect(blocks.every((block) => block.operationId === null)).toBe(true);
  });

  // Reported: "selected multiple surfaces in one operation but when I commit
  // created 5 extrusion operation". One commit stamps one operation id, so the
  // five bodies of one Extrude are one row.
  it("folds the features of one commit into a single row", () => {
    const blocks = featureTimelineBlocks([
      feature("feature-10", "feature-10"),
      feature("feature-11", "feature-10"),
      feature("feature-12", "feature-10"),
      feature("feature-13"),
    ]);
    expect(blocks).toHaveLength(2);
    expect(blocks[0].primary.feature_id).toBe("feature-10");
    expect(blocks[0].members.map((member) => member.feature_id)).toEqual([
      "feature-10",
      "feature-11",
      "feature-12",
    ]);
    expect(blocks[1].primary.feature_id).toBe("feature-13");
  });

  it("counts features for the history cursor, not rows", () => {
    const blocks = featureTimelineBlocks([
      feature("feature-1"),
      feature("feature-2", "feature-2"),
      feature("feature-3", "feature-2"),
      feature("feature-4", "feature-2"),
    ]);
    expect(featureCountBeforeBlock(blocks, 0)).toBe(0);
    expect(featureCountBeforeBlock(blocks, 1)).toBe(1);
    expect(featureCountBeforeBlock(blocks, 2)).toBe(4);
    // A cursor can only land on an operation boundary, never inside it.
    expect(blockEndingAtFeatureCount(blocks, 1)).toBe(0);
    expect(blockEndingAtFeatureCount(blocks, 2)).toBe(-1);
    expect(blockEndingAtFeatureCount(blocks, 4)).toBe(1);
  });
});
