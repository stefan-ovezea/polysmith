# Interaction Performance — investigation and plan

**Status:** investigation complete (2026-10-20), implementation not started.
**Trigger:** an imported DXF sketch (847 lines, 48 detected regions) makes the app
feel broken — "selecting surfaces is close to impossible and slow", with no error
to point at.

This is the weakest point of the product, so it is measured rather than guessed,
and the plan below targets the cause instead of the symptom.

## 1. Measurements

Taken on the reported file (`projects/Sketch3.dxf`, 847 lines after the import
fix, 48 regions), by the DXF suite, so they re-run on every `pnpm test:core`:

| Operation | Cost |
|---|---|
| parse the DXF | 6 ms |
| import into a sketch | 216 ms |
| sketch refresh (one edit) | 106 ms |
| viewport state build | 11 ms |
| profile detection | **0.1 ms** |
| **document state payload** | **1,219 KB**, 23 ms to serialise |

Profile detection — the obvious suspect — is three orders of magnitude too small
to explain the lag. The payload is the problem, and it is sent on **every**
document change: 225 command handlers in `src/app/impl/` reply with the full
`make_document_state_event`. Eleven of them are *selection* commands.

### Where the 1,219 KB is

| Part | Size | Derived? |
|---|---|---|
| `sketch_parameters.profiles` | **704 KB** | yes — recomputed on every refresh |
| `sketch_parameters.vertices` | **277 KB** | yes — rebuilt from lines/arcs |
| `sketch_parameters.lines` | 212 KB | no — the source geometry |
| `sketch_parameters.arcs` | 21 KB | no |
| everything else | ~5 KB | |

**80 % of the wire format is data the core derives.** A click ships it, the
renderer parses it (`JSON.parse`), validates it against the zod document schema,
and re-renders the scene — for a change that touched one string
(`selected_sketch_profile_id`).

## 2. Root causes

1. **Selection is interaction state, but it is broadcast as document state.**
   `CLAUDE.md` is explicit: *"Core sends DOCUMENT STATE, not INTERACTION STATE.
   If it moves with the mouse, it's UI."* Selection is the textbook case, and
   today it lives in the core document (`selected_feature_id`,
   `selected_sketch_profile_ids`, `selected_sketch_entity_ids`, …) and rides the
   full state event. There is already a precedent for the better pattern: the
   CAM panel's "Apply" sends explicit ids precisely because *"the core does not
   read its own selection state"*.
2. **Derived caches are serialised.** `profiles` and `vertices` are recomputed on
   every refresh; they have no business on the wire. The UI reads them only for
   the **active** sketch (editor overlays, extrude defaults, profile labels,
   context menus, AI helpers) — verified by grep, see §5.
3. **One monolithic broadcast.** Any change re-sends everything, so cost scales
   with document size rather than with the change. 847 lines is a normal imported
   sketch; the same path with 10k entities was 14 s per edit before the import
   fix, and would still be seconds per click today.

## 3. What "fix it for good" means

Not "make selection faster": stop shipping geometry when the user points at
something, and stop shipping derived data at all. The end state:

- **A selection change costs one small event** (ids + revision), not a document.
- **The wire carries source geometry, not caches**: `profiles`/`vertices` are
  sent only where they are actually consumed (the active sketch), as counts and
  bounds elsewhere.
- **Cost scales with the change**, not with the document.

## 4. Plan

### Phase A — trim the wire format (biggest win, lowest risk)

`to_payload(DocumentState)` includes `profiles` and `vertices` for every sketch.

- Serialise `profiles` and `vertices` **only for the active sketch**
  (`active_sketch_feature_id`); for other sketches send `profile_count`,
  `line_count`, `bounds`.
- Keep `lines`/`arcs`/`constraints`/`dimensions` (source data, and the sketch
  editor needs them when the sketch becomes active).
- The zod schema makes both fields optional-with-default, so the UI keeps
  working; the sketch editor is only ever open on the active sketch.

Expected: 1,219 KB → **~250 KB** for a 847-line sketch, and near-constant for
the *other* sketches in a document. Removes 80 % of the bytes with no protocol
shape change and no UI change.

**Risk:** UI reads `profiles`/`vertices` on a non-active sketch. §5 lists every
site; each is either active-sketch-only or can use the new counts.

### Phase B — selection-only events (the architectural fix)

New event `selection_changed`:

```json
{ "id": "...", "type": "selection_changed", "payload": {
    "revision": 42,
    "selected_feature_id": "feature-7",
    "selected_reference_id": null,
    "selected_face_id": null,
    "selected_edge_ids": [],
    "selected_sketch_profile_ids": ["polygon-3"],
    "selected_sketch_entity_ids": [],
    "selected_sketch_vertex_ids": [],
    "selected_sketch_dimension_id": null,
    "selected_sketch_entity_id": null,
    "selected_sketch_vertex_id": null,
    "active_sketch_tool": "select"
} }
```

- **Core:** the 11 `select_*` handlers reply with this event instead of
  `document_state`. The core keeps ownership of selection (commands still act on
  it); only the *transport* changes.
- **UI:** `handleCoreMessage` patches exactly those fields into the store's
  document copy, bumps `revision`, and skips zod validation of geometry. One
  small zod schema for the selection payload — cheap and it keeps the schema
  discipline.
- **Why not move selection to the UI?** That is the cleaner long-term shape, but
  it means every command that acts "on the selection" needs explicit ids — a
  much larger refactor. Phase B gets the interaction cost down to bytes while
  keeping the door open (the event is already the ids).

Expected: a click goes from 1,219 KB to **< 1 KB**.

### Phase C — pick regions from a list, not by ray-casting 48 overlapping areas

The extrude panel gets a **region list**: the detected regions with their area
and vertex count, hover-highlighting the region in the viewport, click to
select, and an **isolate** toggle that hides the others. Data source: the
**viewport state** already carries `sketch_profiles` primitives (id, points,
plane frame) — add `area` and `bounds` there (small, per-region), not in the
document.

This is what makes a 48-region sketch usable at all; it does not change the
geometry, and it needs no document change.

## 5. Every UI read of the derived fields

Verified by grep (`sketch_parameters.profiles|vertices`):

| Site | Needs |
|---|---|
| `lib/aiCommandProtocol.ts`, `lib/ai/profileAssertions.ts` | active sketch only (AI prompt context) |
| `app/extrudeDefaults.ts` | active sketch only (default profile) |
| `app/sketchSourceLabels.ts` | active sketch only (profile labels) |
| `layout/ViewportPanel.tsx` (point count) | count only → use `line_count`/`vertex_count` |
| `layout/viewport/contextMenuState.ts` (vertex hit) | active sketch only |
| `lib/featureDependents.ts` | presence check → count suffices |

So Phase A is safe if non-active sketches carry **counts** instead of arrays.
That table is the contract to hold the change to.

## 6. Verification

- **A/B measured, not asserted by eye**: extend the DXF suite to print the
  payload size and its per-key breakdown (already does) and assert a ceiling
  (e.g. < 400 KB for this document). Wall-clock assertions stay out of tests;
  **size** assertions are deterministic.
- **B**: a test that a selection command replies with `selection_changed` and
  that the payload is < 1 KB; a vitest for the store patch (selection fields
  applied, geometry untouched).
- **C**: vitest for the region list ordering/formatting; manual check that hover
  highlights the right region.
- **Regression net**: `pnpm test:core` (73 suites) + `tsc` + the app: delete a
  line, click through regions in the extrude panel, undo/redo (selection events
  must not break the undo path).

## 7. Deliberately not doing

- **Compressing the JSON.** It hides 704 KB of cache instead of removing it.
- **Debouncing selection.** Makes a broken interaction feel intentional.
- **Capping the number of regions.** Hides real geometry and will bite on a
  legitimate 200-region sketch.
- **Sending profiles only for the selected sketch.** Tempting, but the picker
  needs all regions to draw them; back to Phase C, which needs area/bounds only.

## 8. Order of work

1. **A** (wire trim) — one core change + schema defaults; the numbers move most.
2. **B** (selection events) — core event + UI patch; the click becomes instant.
3. **C** (region list) — UI feature; makes a messy sketch pickable.

A and B are independent (either can ship first); C is independent of both.
