import type { DocumentState } from "@/types";

export type TimelineFeature = DocumentState["feature_history"][number];

/**
 * One row of the timeline.
 *
 * A multi-surface extrude creates one body per disconnected group, and a body's
 * id IS its feature id in this codebase, so it cannot be a single feature
 * without changing body identity everywhere (fillet targets, CAM references,
 * `feature-N:face:M`). The core therefore stamps every feature of one commit
 * with the same `operation_id`, and this module folds those into ONE row, which
 * is what Fusion shows: one Extrude, N bodies.
 */
export interface TimelineBlock {
  /** Shared operation id, or null for a standalone feature. */
  operationId: string | null;
  /** Members in history order; members[0] is the one the block acts on. */
  members: TimelineFeature[];
  primary: TimelineFeature;
}

export function featureTimelineBlocks(
  features: readonly TimelineFeature[],
): TimelineBlock[] {
  const blocks: TimelineBlock[] = [];
  for (const feature of features) {
    const operationId = feature.operation_id ?? "";
    const previous = blocks[blocks.length - 1];
    // Consecutive only: a group split by another gesture is two rows, which
    // keeps the timeline's order honest even if an id is ever reused.
    if (operationId !== "" && previous?.operationId === operationId) {
      previous.members.push(feature);
      continue;
    }
    blocks.push({
      operationId: operationId === "" ? null : operationId,
      members: [feature],
      primary: feature,
    });
  }
  return blocks;
}

/**
 * How many features the history cursor includes when it sits before this block.
 * The core's `timeline_cursor` counts features, not rows, and an operation is
 * atomic: a cursor can sit before or after it, never inside it.
 */
export function featureCountBeforeBlock(
  blocks: readonly TimelineBlock[],
  blockIndex: number,
): number {
  let count = 0;
  for (let index = 0; index < blockIndex && index < blocks.length; index += 1) {
    count += blocks[index].members.length;
  }
  return count;
}

/** The block that ends at this included-feature count, or -1 for none. */
export function blockEndingAtFeatureCount(
  blocks: readonly TimelineBlock[],
  featureCount: number,
): number {
  for (let index = 0; index < blocks.length; index += 1) {
    if (featureCountBeforeBlock(blocks, index + 1) === featureCount) {
      return index;
    }
  }
  return -1;
}
