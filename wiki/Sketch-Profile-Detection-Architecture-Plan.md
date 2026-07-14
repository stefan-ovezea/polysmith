# Sketch Profile Detection — Architecture Plan

## Current State (commit 5240597)

`build_sketch_profile_regions` has three paths that all feed into `profiles`:

1. **Arrangement path** (lines 50-130): polygons circles + arcs into 64-segment
   polylines, detects arrangement faces. Handles tangent lines via explicit
   vertex splits (`split_circle_segments_at_line_endpoints`,
   inline arc split logic). Produces polygon (approximate) faces.

2. **Edge-split + edge-loop path** (lines 151-281): splits arc ProfileEdges at
   tangent points, then runs `detect_edge_loop` which requires exactly 2 edges
   per node (manifold topology). **Dead code when `use_arrangement` is true**
   because the arrangement runs first and the early return skips it.  Even
   when it runs, T-junctions (3 edges/node at tangent points) cause
   `detect_edge_loop` to reject the component.

3. **Circle-only path** (lines 289-368): handles standalone circles, inner loop
   nesting. Skipped when `use_arrangement` returns early.

## Problems

| Path | Issue |
|------|-------|
| Arrangement | Arc polyline is an open curve — can't form a closed face alone. Works when lines close the curve, but the face is approximate (64-seg polygon). |
| Edge-loop | Tangent points create T-junctions (3 edges/node). `detect_edge_loop` requires exactly 2 edges/node → rejects. |
| Circle-only | Skipped by early return when arcs are present, losing circle nesting. |

## Proposed Hybrid Architecture

### Core idea: keep both paths, let each handle what it does best.

```
build_sketch_profile_regions:

1. [NEW] circle_arrangement_profiles (lines + circles, no arc involvement)
   → polygon arrangement faces for circle wedges + circle interiors
   → same as today's arrangement path but ONLY for circles

2. [EXISTING, REPAIR] arc_edge_loop_profiles (lines + arcs)
   → split arc edges at line intersection points (existing edge-split code)
   → run detect_edge_loop on each component (existing)
   → FIX: detect_edge_loop must handle T-junctions

3. [EXISTING] circle_only_profiles (standalone circles, nesting)
   → same as today, always runs (no early return to skip it)

4. [NEW] fallback_arrangement (for mixed arc+circle sketches)
   → only when BOTH arcs and circles are present AND edge-loop fails
   → polygon arrangement as last resort
```

### Step 1: Fix `detect_edge_loop` for T-junctions

The function currently requires `nodes.size() == edges.size()` (each node has
exactly 2 incident edges). For tangent arcs, nodes at the tangent point have
3 incident edges.

**Fix**: Allow nodes with >2 edges when the extra edges belong to the same
arc entity.  The arc continuation past the tangent point is still a single
logical curve.  In the edge-loop trace, treat the arc as passing through the
tangent point without "turning."

Alternatively: for each tangent node, "merge" the two arc edges (before and
after the tangent) into a single logical edge for the purpose of degree
checking.  The node then has degree 2 (line + merged-arc) and the loop can
close.

### Step 2: Separate circle arrangement from arc processing

Move the arc polyline generation out of the arrangement path. The arrangement
path should only handle circles. Arcs go through the edge-loop path (with
the T-junction fix).

### Step 3: Add fallback for mixed sketches

When a sketch has both circles and arcs, and the edge-loop path rejects an
arc component, fall back to polygon arrangement for that component only
(rather than the whole sketch).

## Files to modify

- `native/cad-core/src/core/sketch/impl/build_sketch_profile_regions.inc`
- `native/cad-core/src/core/sketch/impl/detect_sketch_profiles.inc`
- `native/cad-core/src/core/sketch/impl/sketch_profile_edge_loops.inc`
  (`detect_edge_loop` needs T-junction handling)

## Risks

- T-junction fix in `detect_edge_loop` is subtle. Must ensure it doesn't
  create spurious faces or miss valid ones.
- Separating circle and arc paths may break sketches where a single face
  involves both a circle and an arc (rare in v1, but possible).
- The arc polyline splitting code is duplicated (arrangement path and
  edge-split path). Should be extracted to a shared helper.

## Verification

- Add test: lines tangent to arc → wedge profile detected
- Add test: arc + line + arc closed loop → profile detected
- Regression: all existing sketch_profile_test cases pass
- Regression: multi_profile_extrude_test passes
