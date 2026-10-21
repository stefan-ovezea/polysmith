# IPC payload plan — phases C to F

Status: **plan finalized** 2026-10-21. Extends `wiki/Interaction-Performance.md`
(phases A and B are shipped: `7835021`, `3c0552b`).

Phases A and B fixed the *click*. What remains is the **edit**: a command that
changes the document still re-serialises everything, and the viewport snapshot
is larger than the document. This plan closes both, with numbers measured on
this machine against the built core.

## 1. Measured baseline

`scripts/measure_payload.py` drives `native/cad-core/build/Release/cad_core.exe`
over its own JSON protocol and records the bytes on the wire. On
`projects/Sketch3.polysmith` (3.28 MB on disk, 847-line sketch, one extrude):

| command | reply | bytes |
|---|---|---:|
| `create_document` | `document_created` | 2,354 |
| `load_document` | `document_state` | 574,025 |
| `get_document_state` | `document_state` | **574,025** |
| `get_viewport_state` | `viewport_state` | **1,617,965** |
| `reenter_sketch` (open) | `document_state` | **1,692,105** |
| `select_sketch_entity` | `selection_changed` | **476** |
| `select_sketch_vertex` | `selection_changed` | **480** |
| `update_sketch_vertex` (one point) | `document_state` | **1,439,408** |
| `get_document_state` (after the edit) | `document_state` | 1,439,408 |

**The worst case is not a click, it is an edit on the sketch you have open:
1.44 MB for moving one point.** Phases A and B fixed selection (476 bytes); they
do not touch this, because Phase A deliberately keeps the derived caches for the
*active* sketch — and the active sketch is exactly where editing happens.

Run it yourself:

```powershell
python scripts/measure_payload.py
python scripts/measure_payload.py --document projects/part-stefan-new.polysmith
```

### 1.1 Where the 574 KB of document_state is

| item | bytes | derived? |
|---|---:|---|
| `feature_history` → `feature[extrude]` | 356,859 | mixes source and snapshot |
| `feature_history` → `feature[sketch]` | 258,538 | source (the sketch) |
| — `sketch_parameters.lines` | 234,661 | no, this is the geometry |
| — `sketch_parameters.arcs` | 21,875 | no |
| everything else (`selection_filter`, `drawing`, `cam`, `appearance`, ids) | < 1,000 | — |

Inside the extrude's 356 KB:

| `extrude_parameters.*` | bytes | what it is |
|---|---:|---|
| `sketch_lines` | **214,357** | a **verbatim copy of the sketch's lines** |
| `additional_profile_points` | 28,591 | frozen profile geometry |
| `profile_points` | 24,989 | frozen profile geometry |
| `boundary_edges` | 20,717 | frozen edge references |
| `additional_boundary_edges` | 17,460 | frozen edge references |
| `sketch_arcs` | 14,615 | copy of the sketch's arcs |
| `profile_ids` + `profile_id` | 17,369 | ids (these are the useful part) |
| misc ids (`sketch_edge_ids`, `additional_*`) | ~11,000 | ids |

**The same geometry crosses the wire twice** — once as the sketch that owns it,
once as the snapshot the extrude stores. Every additional extrude, revolve,
sweep or loft over the same sketch adds another copy.

Phase A is confirmed working: outside the active sketch, `profiles` and
`vertices` are present but empty (2 bytes each), so no derived caches leak.

### 1.1b With the sketch open (the case that hurts)

Opening the sketch (`reenter_sketch`) and then moving one point:

| item | bytes | derived? |
|---|---:|---|
| `sketch_parameters.profiles` | **915,619** | yes — recomputed on every refresh |
| `sketch_parameters.vertices` | 306,191 | yes — rebuilt from lines/arcs |
| `sketch_parameters.lines` | 234,634 | no |
| `feature[extrude]` (`extrude_parameters` + arcs) | 79,597 | snapshot |
| `feature[root_part]`, filters, ids | < 2,000 | — |
| **total per edit** | **1,439,408** | **85 % of it is derived** |

So while a sketch is open, every point move, dimension change or trim ships
~1.2 MB of data the core itself recomputes. That is the single worst path in the
protocol, and it is the one the user feels (drag commit, dimension entry, trim).

### 1.2 Where the 1.62 MB viewport_state is

| key | bytes |
|---|---:|
| `sketch_profiles` | **912,800** |
| `sketch_lines` | 311,647 |
| `solid_faces` | 206,315 |
| `meshes` | 173,302 |
| `edges` | 100,002 |
| `vertices` | 37,100 |
| `sketch_arcs` | 26,975 |

`sketch_profiles[0]` carries `boundary_edges` (1.5 KB for one region alone),
`profile_points`, `plane_frame` … for all 48 regions. The picker needs an **id,
area and bounds** per region (that is what Phase C of the older plan wanted);
the full point lists are only needed for the region under the cursor.

## 2. Phase C — stop shipping the profile snapshot (biggest win, lowest risk)

The extrude-style features keep a frozen copy of the profile they were built
from. That copy is *derived*: it can be rebuilt from the sketch it names
(`sketch_feature_id` + `profile_ids`). It must stay in **saves** (a reload has to
re-resolve profile references before any refresh), exactly like
`profiles`/`vertices` in Phase A.

**Core.** Thread a flag the way Phase A did:

```cpp
json to_payload(const FeatureEntry& feature, bool include_opaque,
                bool include_derived_geometry, bool include_profile_snapshot);
```

Omit from **events** (keep in saves):

- `extrude_parameters`: `sketch_lines`, `sketch_arcs`, `sketch_circles`,
  `profile_points`, `additional_profile_points`, `boundary_edges`,
  `additional_boundary_edges`, `inner_loop_edges`, `additional_inner_loops`,
  `intersect_result`
- the same fields on `revolve_parameters`, `sweep_parameters`, `loft_parameters`

Keep: `profile_id`, `profile_ids`, `profile_kind`, `sketch_feature_id`,
`plane_id`/`plane_frame`, `depth`, `mode`, `side1`/`side2`, `target_body_id`,
plus a new `profile_point_count` so consumers can show a size.

**UI.** Mark the omitted fields optional in the type
(`FeatureHistoryEntry["extrude_parameters"].sketch_lines?`). Nothing in the UI
reads them on the hot path today — the preview uses `depth`, `mode`,
`target_body_id`, `profile_ids`, `sketch_feature_id`. Where geometry *is* needed
(the extrude dialog's preview), rebuild it from the sketch feature the extrude
names, and only fall back to a `get_feature_detail(feature_id)` request for
legacy designs.

**Expected:** 574 KB → **~210 KB** for this document (sketch source remains), and
a document with *n* extrudes over one sketch stops growing by 350 KB per
feature.

**Risk:** a design saved before the change still carries the snapshot; the reader
must accept it present or absent (`has_value` / `contains`), never assume.

## 2b. Phase C2 — stop re-sending the open sketch's derived caches on every edit

Phase A made the caches ship for the *active* sketch only, on the reasoning that
the editor needs them. It does — but it needs them when the sketch **opens** and
when its **topology changes**, not on every point move. Today one move costs
1.44 MB, 85 % of it `profiles`/`vertices`.

**Core.** Add a `derived_revision` (bump it when profile detection or the vertex
table actually changes) and ship the arrays only when one of these holds:

1. the command opens the sketch (`reenter_sketch`, sketch creation, `load_document`
   with `include_opaque`);
2. `profile_count` or `vertex_count` differs from what the client last saw — the
   plan's suggestion is to compare against the previous payload the core built;
3. the client asks: `get_sketch_derived { feature_id }` (returns profiles +
   vertices + `derived_revision`).

Otherwise the payload carries `profile_count`, `vertex_count`,
`derived_revision`, and nothing else derived.

**UI.** Keep the last derived payload per open sketch; after an edit, if
`derived_revision` is unchanged, keep what it has (a point move does not change
regions). If it changed, fetch `get_sketch_derived` once — a topology edit is
rare compared with a drag or a dimension edit.

**Expected:** 1.44 MB → **~310 KB** for a point move on this sketch (lines +
arcs + the extrude snapshot), and to **1–3 KB** once Phase D lands.

**Risk:** any UI path that reads `profiles` without a refresh. The older plan's
§5 table lists every such site; hold the change to that contract and let the
conformance harness catch a missed one.

## 3. Phase D — feature and entity deltas (the "cost scales with the change" step)

After Phase C, an edit still ships the whole timeline. Two new events, following
the shape Phase B established (`selection_changed`):

```json
{ "id": "...", "type": "feature_changed", "payload": {
    "revision": 43,
    "feature_id": "feature-7",
    "feature": { "feature_id": "feature-7", "kind": "sketch", "sketch_parameters": { } },
    "removed_feature_ids": []
} }
```

```json
{ "id": "...", "type": "sketch_entities_changed", "payload": {
    "revision": 44,
    "feature_id": "feature-7",
    "lines": [ { "line_id": "line-118", "start_x": 37.42, "start_y": -8.15 } ],
    "vertices": [ { "vertex_id": "vertex-31", "x": 37.42, "y": -8.15 } ],
    "removed": [ "line-102" ],
    "derived": { "profile_count": 48, "vertex_count": 412 }
} }
```

**Core:** one helper beside the selection one, used by the handlers that touch a
single feature:

```cpp
json reply_feature_changed(const FeatureEntry& feature, uint64_t revision);
json reply_sketch_entities_changed(const std::string& feature_id,
                                   const std::vector<SketchLineEntry>& lines,
                                   const std::vector<SketchVertex>& vertices,
                                   const std::vector<std::string>& removed,
                                   uint64_t revision);
```

Adopt per family, highest traffic first: sketch geometry and dimensions →
extrude/revolve parameters → the rest opportunistically. `document_state` stays
the **resync** path: open, new, undo/redo, import, or a revision gap
(`event.revision !== state.revision + 1`).

**UI:** patch `feature_history` by `feature_id`; for entity events patch inside
the open sketch only. If the id is unknown, request a full state instead of
inventing an entry.

**Expected:** a vertex move on the 847-line sketch: 574 KB → **1–3 KB**
(the one line plus its two vertices). A parameter edit on a small document:
< 1 KB.

**Risk — this is the one that matters:** incremental state is how a UI drifts
from its core. Do §5 first.

## 4. Phase E — viewport budget

Ordered by value per day:

1. **Region list, not region geometry** (the biggest single item, 912 KB).
   `sketch_profiles` entries become `{profile_id, profile_kind, area, bounds,
   plane_id, plane_frame, is_selected}`; full `profile_points`/`boundary_edges`
   are fetched for the hovered/selected region only
   (`get_profile_geometry {feature_id, profile_id}`), or rebuilt client-side from
   `sketch_lines`, which are already on the wire.
   *Expected: 1.62 MB → ~700 KB.*
2. **Skip when geometry did not change.** Dimension/parameter edits that do not
   alter a body must not trigger a viewport refetch. The UI already skips it for
   selection setters; extend the same rule to the new delta events.
3. **Coalesce:** at most one viewport snapshot per revision; drop stale ones.
4. **Binary buffers:** positions/normals/indices as base64 `f32`/`u32` instead
   of JSON number arrays. Three.js consumes typed arrays directly, so this
   removes a parse-and-widen pass as well as bytes: typically 3–5× smaller.
   *Expected: meshes 173 KB → ~50 KB; solid_faces/edges/vertices 343 KB → ~90 KB.*

## 5. The guardrail (do this before Phase D)

**Shipped 2026-10-21: `scripts/ipc_conformance.py`.** It replays a command
script against the core, builds a **shadow client view** by applying the intended
merge rules to each event, and asserts the shadow equals a full `document_state`
produced by replaying the same mutations in a fresh process. The rules live in
one function (`merge_document`), so the UI and the harness cannot drift apart
silently:

- `document_state` replaces the view, but a feature that omits
  `profiles`/`vertices` keeps the arrays the client already had;
- `feature_changed` patches one feature (same keep-derived rule);
- `sketch_entities_changed` patches lines/vertices in one feature;
- `selection_changed` patches the selection fields;
- any other event is a failure — no merge rule means no silent lie.

`--self-test` corrupts the last full state and expects a mismatch, so the harness
cannot rot into a no-op:

```powershell
python scripts/ipc_conformance.py              # conformant, exit 0
python scripts/ipc_conformance.py --self-test  # injects a bug, expects to catch it
```

Verified output today: 7 events merged, `shadow vs full state 0 diff(s)`, and the
self-test reports `document.feature_history[1].sketch_parameters.lines:
length 845 != 846`.

The remaining part of the guardrail is a core-side **size assertion** next to the
existing DXF payload assertions: `document_state < 250 KB` for the trimmed case,
and `< 2 KB` for `selection_changed` and `sketch_entities_changed` (sizes are
deterministic; wall-clock assertions are not).

## 6. Order of work, with expected results

| step | change | expected on Sketch3 | effort |
|---|---|---|---|
| 0 ✅ | conformance harness (`scripts/ipc_conformance.py`, self-test) | conformant, catches injected drift | **done** |
| 1 ✅ | Phase C2: open-sketch derived caches on demand | edit **1.44 MB → 599 KB** | **done** |
| 2 ✅ | Phase C: profile snapshot out of events | load **574 KB → 276 KB**; edit 599 → **549 KB** | **done** |
| 3 | Phase D1: `sketch_entities_changed` for the open sketch | edit → 1–3 KB | 3–5 d |
| 4 | Phase D2: `feature_changed` for the other families | 276 → 5–20 KB typical | 2–3 d |
| 5 | Phase E1/E2: region list, skip unchanged viewports | 1.62 MB → ~700 KB, often 0 | 2–3 d |
| 6 | Phase E4: binary buffers | another 3–5× | 2 d |
| 7 | Phase F: validate the envelope always, geometry arrays dev-only | removes a `zod` pass per event | 0.5–1 d |

Steps 0–2 are shipped and measured. Skipping the staged compromise the plan
originally proposed was the right call: the profile snapshot could be dropped
outright because `preserve_extrude_source_geometry`
(`core/document/impl/private_extrude_profile_parameter_helpers.inc`) already
copies every snapshot field from the core's own copy on a parameter update, so a
trimmed wire cannot wipe it, and saves still write it.

Remaining loose end from step 1: `get_sketch_profiles` exists but nothing calls
it yet. Consumers use the set that arrives when a sketch opens; after a topology
edit *while open* the profile ids can lag until re-entry. Pure moves are fine
(region ids do not change). Wire it into `extrudeDefaults`,
`sketchSourceLabels` and `sketchSelectionDelete` when either needs to be exact.

After steps 0–5 the reported sketch is single-digit milliseconds per edit, with
no architectural change.

## 7. Deliberately not doing

- **gzip** — hides caches instead of removing them and adds a decompress pass on
  the hot path (the older plan says the same).
- **debounce** — makes a broken interaction feel deliberate.
- **capping the number of regions/lines** — hides real geometry and will bite on
  a legitimate 200-region sketch.
- **moving the drag into the core** — even with a slim document payload, every
  `update_sketch_point` also issues `get_session_state` + `get_viewport_state`
  (1.6 MB, ~11 ms build). The document payload was never the drag blocker; the
  viewport snapshot is. Client-side preview stays.
