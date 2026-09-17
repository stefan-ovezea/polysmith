# Trim Tool Redesign — Requirements & Design

> **Status:** Implemented (2026-09-17) — all four phases (engine rebuild,
> constraint transfer + projection pruning, Corner/Extend/Split,
> drag-paint stroke) are complete on `feature/trim` and user-verified
> in-app. Gated by the stage test program
> (`cad_core_trim_stages_test --stage 1..8`, 51/51 suites) + tsc.
> Supersedes the "Trim tool — known issues (revisit needed)" backlog entry
> in the Implementation-Log; the interaction notes in
> [Trim-Tool-Implementation-Plan.md](Trim-Tool-Implementation-Plan.md)
> remain the reference for the preview/race contract, which stays as-is.

## Why a rewrite

The 2026-05 trim tool was designed for line/circle/arc before the project
grew ellipses, splines, fillets, chamfers, parameters, projections and the
profile-exact walk. Five years of patches accumulated structural debt that
no point-fix addresses:

- five divergent cleanup pipelines (isolated-delete paths leak fillets,
  anchors and line relations that the normal path removes),
- minted trim points are frozen **after** the solver pass that can move them,
- an iterator used after `lines.push_back`,
- spline re-fit throws after the undo push,
- hardcoded legacy quadrant vertex ids (`vertex-<circle>-quadrant-N`),
- nine copies of the intersection dispatch with inconsistent
  endpoint-touch passes,
- zero-length lines guarded after the fact instead of prevented at source,
- asymmetric preview payloads between entity kinds.

## Industry baseline (researched: Fusion 360, SolidWorks, FreeCAD, Onshape)

| Behavior | Standard |
|---|---|
| Click deletes the segment between the nearest intersections | unanimous |
| No intersections ⇒ the whole entity is deleted | unanimous |
| Hover preview paints exactly what dies, before the click | unanimous |
| Drag/paint trim across many entities in one stroke | unanimous |
| Circle trims to an arc | unanimous |
| Construction geometry acts as a cutting edge | unanimous |
| Companion Extend and Split tools (separate commands) | unanimous |
| One undo step per operation | unanimous |
| Corner trim (two entities → virtual corner, consumes both) | SolidWorks only |
| Right-click cycling of ambiguous picks inside trim | none of the four |

## Requirements

### R1 — Trim (segment delete)

Kept as the core interaction, already at industry parity: core-computed
preview (`trim_preview` → `trim_preview_result`), revision guard, click or
`segment_index` commit, continue mode, Escape exits, nothing left selected.
Rebuilt on the new engine (below).

### R2 — Corner trim (new)

Pick two entities; both are trimmed/extended to their virtual corner.
Hover preview on the second pick before commit. Consumes both sides in
one operation — the profile-repair case the user hits when closing a
corner (e.g. after deleting a fillet).

### R3 — Extend (new)

Extends the clicked entity to the **first intersection** in the extension
direction. No free-space extension (Fusion/SolidWorks behavior, not
FreeCAD/Onshape).

### R4 — Split (new)

Divides the clicked entity at all intersections without deleting anything.
Circle/full-ellipse need two split points (SolidWorks rule).

### R5 — Drag-paint trim (new)

One stroke trims every segment the pointer crosses, committed as **one
undo entry** (batch command; per-entity sub-undo inside the stroke is
explicitly out of scope).

## Design decisions (approved D1–D5)

**D1 — Constraint policy: FreeCAD-style transfer.**
On trim, surviving segments inherit applicable constraints from the
original entity via a strict whitelist:

- coincident (endpoint pairs that survive),
- horizontal / vertical,
- parallel / perpendicular (to the same reference),
- equal (to the same reference),
- point-on-object: the surviving endpoint on its cutting curve gets a
  point-on-object constraint at the cut (industry: Fusion creates
  coincident, FreeCAD point-on-object — we follow FreeCAD).

Dimensions on surviving geometry re-derive as **driven** (measured), not
stale drivers. Fillet and chamfer records are always removed when an
operand or their owned arc/line/trim point dies — owned geometry cannot
outlive its record. Explicit Fix constraints do not transfer (they pin
specific ids).

**D2 — Extend to the first intersection only.** No arbitrary extension in
v1.

**D3 — No right-click segment cycling inside trim.** None of the four
CADs has it; skip (can be added later without architecture change).

**D4 — Trimmed circle keeps concentric/equal relations** to other circles
when the result is an arc (Solid Edge behavior).

**D5 — Rebuild in place.** New engine internals behind the existing IPC
contract (`trim_preview`, `trim_sketch_entity`) and the existing test
suites as the safety net. No parallel subsystem, no UI contract changes
until the new tools (R2–R5) need them.

## Engine architecture

1. **One intersection authority.** All pairs go through the shared
   `sketch_curve` layer (analytic line/circle/arc/ellipse; OCCT for
   spline pairs) with ONE dispatch and identical endpoint-touch passes
   for every target kind. `kTrimCoincidentTolerance == kProfileTolerance`
   stays — trim and the profile walk must never disagree.
2. **One cleanup pipeline.** A single post-trim pass handles dimensions,
   constraints, line relations, midpoint/point-line anchors, and
   fillet/chamfer records for every path (normal split, isolated delete,
   single-survivor, all kinds).
3. **Constraint transfer module.** Runs inside the trim commit, before
   the solver pass, and re-anchors the D1 whitelist onto surviving ids.
4. **Freeze before solve.** Minted split vertices are frozen the moment
   they exist, before `refresh_sketch_derived_state` runs planegcs.
5. **Zero-length at source.** Split math never emits a degenerate piece —
   near-identical intersection parameters collapse to one point and the
   piece is skipped; the global zero-length cleanup stays only as a
   safety net for non-trim paths.
6. **Stable ids.** `next_trim_entity_index` allocation stays; quadrant
   points get real `vertex-<n>` ids (kill the legacy hardcodes); the
   ellipse/spline live-point nets are replaced by the unified cleanup.
7. **Undo safety.** Every failure path throws BEFORE `push_undo_state`;
   each trim commit is one undo entry; R5's stroke batches into one.

## Phases (each gated: build + full suite + tsc)

- **Phase 0 — engine rebuild, no behavior change.** Intersection
  unification, single cleanup pipeline, freeze-before-solve, zero-length
  at source, quadrant ids, iterator/undo fixes, preview payload symmetry.
  Gate: existing 24-case `cad_core_trim_test` + all 50 suites.
- **Phase 1 — constraint transfer + projection pruning.** D1/D4 transfer
  module, driven-dim re-derivation, projection generated-id pruning on
  trim. Gate: new regression tests + all suites.
- **Phase 2 — Corner trim, Extend, Split.** Core + UI tools, schema +
  IPC docs.
- **Phase 3 — Drag-paint trim.** Batch command, one undo entry, stroke
  preview.

## Doc conflicts to fix during the implementation

- `wiki/Glossary.md` says trim clicks the portion "you want to keep";
  `help/trim.md` says click-to-delete. The click-to-delete wording is
  correct — fix the Glossary.
- `wiki/V1-Roadmap.md` promised "constraint re-evaluation" while
  `help/trim.md` documented delete-everything. D1 resolves this; update
  both to describe the transfer whitelist.

## Out of scope (recorded, not part of this rewrite)

- Fusion-style **consume-short-line** for fillets (fillet feature work,
  tracked separately).
- **Unify lines** (merge collinear fragments) — a separate tool; Corner
  trim covers the main repair case meanwhile.
- Boundary-region trim (SolidWorks "trim away inside/outside") — v2.
