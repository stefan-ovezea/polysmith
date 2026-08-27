# Trim Tool — Implementation Notes

> **Status:** Modernized (2026-08). The original 2026-05 design covered
> line / circle / arc only; this page reflects the current engine. Trim
> now supports **line, circle, arc, ellipse and spline targets**, with
> every entity kind acting as a cutting edge, through one shared
> exact-curve layer used by both the trim engine and the profile walk.

## What trim does

Splits the clicked entity at every intersection with other
non-construction entities and deletes the segment under the click
(or the highlighted `segment_index` from the core's `trim_preview`
result). One trim affects one entity; other entities act only as
cutting edges. Construction geometry cuts; it never forms profiles.

## Entity behaviour

| Entity | After trim |
|---|---|
| Line | Shortened, or split into two when a middle segment is deleted; a lone surviving segment deletes the line. |
| Circle | Converted to the complementary arc (2+ intersections); a tangent touch leaves the circle whole. |
| Arc | Shortened, or split into two when a middle segment is deleted. |
| Ellipse (full) | Converted to a partial elliptical arc (`has_sweep`, sweep angles, `ccw`, split endpoint vertex ids). |
| Ellipse (partial) | Shortened, or split into two elliptical arcs when a middle segment is deleted. |
| Spline | Shortened, or split into two splines when a middle segment is deleted; sub-curves are re-fit by OCCT's exact knot-insertion segment (no approximation), so cut ends land exactly on the intersection. |

Splines and ellipses act as cutting edges for every target kind.
Intersections come from the shared `core/sketch/sketch_curve` module
(line/circle/arc analytic, line×ellipse analytic, the rest through
OCCT), so the trim engine and the profile walk can never disagree on
intersection geometry or tolerance (`kProfileTolerance = 0.01 mm`).

## Point identity

Split endpoints resolve through `resolve_shared_point`
(`impl/private_point_profile_helpers.inc`) — nearest-wins within the
walk's own tolerance, with an exclusion set so entities under
construction can never match their own endpoints. Unresolved points
mint through one tracked helper, and the document layer freezes the
minted vertices after the refresh so the planegcs pass cannot move a
split point and re-open the loop. Endpoint coordinates stay exactly
on the analytic intersection — the walk's touch/union records fire
deterministically.

## Preview / race contract

- `trim_preview { entity_id, cursor_x, cursor_y }` →
  `trim_preview_result { entity_id, entity_kind, hovered_index,
  revision, full_*?, segments[] }`. The UI renders the red highlight
  exclusively from this payload (no local TS geometry), coalesces
  hover requests to one per frame, and drops responses that are not
  the newest request.
- `trim_sketch_entity` accepts `segment_index` (deletes exactly the
  highlighted segment), plus `expected_revision` / `preview_id`. When
  `expected_revision` does not match the current document revision the
  core IGNORES the stale index and re-derives the segment from the
  click point — a stale preview can never cut the wrong piece.
- The UI gained the app's first request/response correlation layer
  (`sendCoreCommandAwaited`, keyed by command id).

## Profile walk semantics

The walk drops only the **dangling end pieces** of a curve — a line
crossing a closed curve splits it into regions (the ellipse-chord
surface fix). Exceptions: text path lines drop whole (a baseline must
not slice glyph contours), and construction curves never cut. Partial
ellipses walk like arcs; full ellipses like circles.

## FIX badges

Only an explicit fixed constraint (the Fix tool) renders a FIX badge.
Internally frozen points — trim split points, slot-generated vertices,
ellipse axis points — stay pinned for the solver without a badge.

## Regression suites

`cad_core_trim_test` (23 cases: silent-deletion guards, tangent
circle, coincident circles, split-point identity and freezing, the
six-petal flower with the complete region set, stale-revision click
fallback, ellipse/spline cutting edges, ellipse target trims with
complete region sets, save/load sweep roundtrip, ellipse-chord lens,
full-ellipse-survives-line-piece-trims); `cad_core_ellipse_test`
trim-to-arc; `cad_core_spline_test` trim-split. All 30 suites run
via `pnpm test:core`.
