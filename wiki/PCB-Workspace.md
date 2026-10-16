# PCB / Gerber Workspace

**Status: P1 complete** (Gerber parser, polygonizer, Excellon drills) — **P2
gated on review.** Branch `gerber`. Nothing in this document is user-verified in
the app yet; the design below is the review gate for the document-model part
(P2), while the format layer (P1) is pure and independent.

Goal: turn a PCB fabrication data set (Gerber + Excellon) into CAM operations —
**isolation routing** on a mill and **laser cutting/engraving** — using the
existing operation, post, and GRBL pipelines, without teaching the UI anything
about Gerber.

---

## 1. Decision record: a separate top-level workspace

**Decision: add a sixth top-level workspace, `pcb`, next to
`cad | cam | drawing | grbl | slicer`** (`WorkspaceView`, `App.tsx`), with a
strict producer/consumer boundary:

**Naming:** the id is `pcb`, not `gerber`.  The workspace will also own
Excellon drill files, role/polarity resolution and board registration, and the
name should describe the job the way `grbl` does — not one file format.

| Workspace | Owns |
|---|---|
| **PCB** (new) | Importing fabrication data, layer roles/polarity, board registration, PCB job parameters. **Never generates G-code.** |
| **CAM** | Operations, toolpaths, preview, post-processing. Gains the new PCB operations. |
| **GRBL** | Sending a posted program to the machine. Unchanged. |

PCB's only output is **CAM operations** in the document, exactly the way CAM's
only output is a posted program for GRBL.

**Why not a CAM sub-workspace** (`activeCamWorkspace === "pcb"`, like
milling/turning/printing):

1. The PCB flow starts from **files**, not from model geometry. CAM's binding
   contextual workflow is "select inputs → invoke → floating panel → confirm"
   over sketch profiles, faces, or bodies. There is no sketch, body, or stock
   when you import a Gerber set.
2. The natural PCB UI is a **layer table + role mapping + board preview**
   (top/bottom, fabrication view), not an operation tree.
3. Precedent already exists: **GRBL is a separate workspace that consumes CAM
   output**. PCB is the mirror image — it produces CAM input.
4. Keeping it out of CAM keeps **one** operation model, one preview pipeline,
   one post-processor. A PCB workspace that emitted its own G-code would
   duplicate all three.

**Rejected alternatives:** importing Gerber as *sketch geometry* in CAD (pushes
~16k entities and two ~10k-vertex pours per outer layer through profile
detection — the most regression-prone code in the project — and discards
role/polarity semantics); a separate helper application (bypasses the IPC
contract and splits the post pipeline); a CAM sub-tab (points 1–4 above).

**Consequence:** a Gerber import that only needs viewing/cutting still ends as a
CAM operation, so the PCB workspace is a *setup* workspace, not a fourth
geometry universe.

---

## 2. Scope

**In:** Gerber X1 subset (RS-274X) + Excellon drill files; multi-layer boards
(4+ copper layers); layer role/polarity resolution; polygon assembly with
dark/clear polarity; isolation routing (multi-pass, clearance, tabs);
laser layer operations (contour + fill); drill operations; registration from the
board profile; bottom-side mirroring; diagnostics for skipped constructs.

**Out (for now):** Gerber X2 attribute semantics beyond role/polarity, `%SR`
step-and-repeat, `%AB` blocks, `%IPNEG%`, blind/buried via spans, panelization,
real electrical DRC, pick-and-place/assembly, ODB++/IPC-2581, Gerber *export*.

---

## 3. Inputs: supported subset and how it was chosen

Validated against a real KiCad 10.0.4 plot of a 99.9 × 99.9 mm 4-layer board
(11 Gerber files + 2 Excellon + `.gbrjob`), kept as a local fixture.

| Construct | Fixture evidence | Support |
|---|---|---|
| Format/units `%FSLAX46Y46*%`, `%MOMM*%` | all files | yes — 4.6 leading-zero-omitted, absolute, signed |
| Inch + trailing-zero-omitted | not in fixture | yes (synthetic tests) |
| Aperture macros `%AM%` | `AMRoundRect` with primitives 1/4/20 and `$1+$1` arithmetic | yes — macro evaluator is mandatory (all SMD pads) |
| Standard apertures | 413 `%ADD` definitions across the set | yes (C/R/O/P + macro invocation) |
| Modal state | F_Cu: 15,591 `D01` vs **17** explicit `G01` | yes — modal interpolation/aperture/polarity/region state is the parser core |
| Regions `G36/G37` | 33 blocks across copper + silkscreen; pours of ~4.4k and ~10.5k lines | yes |
| Clear polarity `%LPC` | both silkscreens (`LPD` 2 / `LPC` 1 per file) | yes — applied **in file order** |
| Arcs `G75` + `G02/G03` (I/J offsets) | Edge_Cuts (profile!), F_Cu, silkscreens | yes — tessellated at a tolerance; `G74` single-quadrant too |
| Role metadata in-file | `G04 #@! TF.FileFunction,Copper,L1,Top`, `TF.FilePolarity`, `TA.AperFunction,SMDPad,CuDef`, `TD` | yes — KiCad writes X2 attributes as comments; a pure X1 parser safely ignores them |
| Job file `.gbrjob` | per-file `Path`/`FileFunction`/`FilePolarity`, incl. `SolderMask` = **Negative** | yes — authoritative tier |
| Excellon | `M48`/`FMAT,2`/`METRIC` + `%`, `G90`/`G05`, `TnC<dia>` table with per-tool `TA.AperFunction,...,ViaDrill|ComponentDrill`, **decimal** coordinates, `M30`; NPTH is header-only (zero holes) | yes |
| `%SR`, `%AB`, `%IPNEG%`, X2 `%TF/%TA` statements | absent | skip with a warning; never abort the import |

**Layer role + polarity resolution is three-tier:** `.gbrjob` → in-file
`G04 #@!` attributes → filename heuristics (`-F_Cu`, `-Edge_Cuts`, …). The
middle tier matters because KiCad can plot without a job file, and the mask
polarity trap (`SolderMask` is *negative* — the file describes **openings**) is
exactly what an unqualified filename guess gets wrong.

The tiers do **not** share a vocabulary, so the resolver needs an alias table,
not string equality: the same layer is `Paste,Bot` in the Gerber attribute and
`SolderPaste,Bot` in the job file, `Legend` vs `Silkscreen`, `Profile` vs
`Profile,NP`.  Fixture evidence for both spellings lives in
`native/cad-core/tests/data/pcb/`.

**Degrade, never crash:** any unsupported construct is recorded in the parse
diagnostics (line number + reason), the rest of the file still imports, and the
UI surfaces the list. Malformed/oversized input is bounded and returns an error
result rather than throwing through the document layer.

---

## 4. Architecture

```
Gerber set ─┐
Excellon   ─┴→ core/pcb parser (pure)  → PcbLayer polygons/drills
                                          │
                    document feature `pcb_layer` (path + role + parse cache)
                                          │
                     CAM ops: pcb_isolation | pcb_layer_cut | pcb_drill
                                          │
                          existing preview → post → GRBL workspace
```

### Core module layout

| Path | Responsibility |
|---|---|
| `src/core/pcb/gerber_parser.{h,cpp}` | Tokenizer, format/units, apertures + macro evaluation, commands. No geometry, no I/O policy, no logging. |
| `src/core/pcb/gerber_polygonize.{h,cpp}` | Strokes → capsules, flashes → aperture polygons, regions, in-order polarity booleans. |
| `src/core/pcb/excellon_parser.{h,cpp}` | Drill files (tool table, hits, plated/non-plated). |
| `src/core/pcb/pcb_layer_feature.{h,cpp}` | Document feature: source path, role, parse cache, recompute. |
| `src/core/cam/pcb/{isolation,layer_cut,drill}.cpp` | The CAM operations (mirrors `src/core/cam/laser/*`). |

The parser and polygonizer are **pure** — file text in, `PcbLayer` out, no
logger, no `std::cerr`, no exceptions escaping. Diagnostics travel as data
(`{line, severity, message}`) so the caller logs them through
`polysmith::core::log_*` and the UI can list them. That keeps the parser
unit-testable without a document, an OCCT context, or a Tauri event loop.

### Data model

- `PcbLayer`: role (`copper_top`, `copper_inner`, `copper_bottom`, `mask`,
  `paste`, `silkscreen`, `profile`, `drill_pth`, `drill_npth`), polarity, unit,
  integer-scaled polygons (`outer` + holes), drill hits (x, y, tool ⌀, class),
  bbox, diagnostics.
- Feature `pcb_layer`: holds the source path + role; recompute re-parses and
  re-resolves. Re-importing the same path with changed content is a new
  feature revision, not a silent identity change.
- **TNP:** regions are addressed by a new `PcbRegionAttestation` carrying
  witness data — layer feature id, region ordinal, centroid, area, bbox, loop
  count — mirroring the existing `SketchProfileAttestation` pattern
  (`types/geometry/cam.ts`). Every recompute re-resolves the witness against the
  live polygon set; a miss degrades to `dependency_broken` + warning and the
  operation keeps its parameters. Region ordinals are never trusted on their
  own.

### Geometry engine

**Offsetting needs no new dependency.** The core already owns an
arc-preserving 2D loop offsetter used in production for the laser kerf and the
face-milling inset — `cam2d::offset_closed_loop` (round/miter joins, arc
segments preserved) plus segment cleanup, Douglas-Peucker simplification,
segment clipping and loop-containment/nesting, all in
`src/core/cam/cam2d.h`. Isolation routing is exactly "offset the net copper
rings outward by tool radius + clearance, once per pass", so P4 builds on that
rather than on a polygon-clipping library — and it keeps arcs exact instead of
tessellating them.

What that library does **not** cover is set booleans for polarity assembly
(`union(dark) − union(clear)`).  Two observations keep that from being a
blocker:

- **Cutting does not need booleans.** Every ring is a closed loop to cut,
  independent of polarity; holes are simply more rings.  Only the *side* the
  kerf lands on and fill/engrave parity depend on polarity, and both can be
  derived from per-ring orientation plus the existing containment/nesting
  helpers (`loop_contains`, `clip_segment_*`).
- **The hard case is narrow.** True set booleans are unavoidable where offsets
  from neighbouring traces overlap and must be trimmed, and where a negative
  plane must be assembled.  If a real board proves that necessary, **Clipper2**
  (Boost Software License 1.0 — permissive, AGPL-compatible) is the candidate,
  vendored in `third_party/clipper2` like zlib/libharu/nlohmann, with an
  integer scale coarser than the 1 µm Gerber resolution.  That decision is
  deliberately deferred to P4 and needs board evidence *before* a dependency is
  taken on.

OCCT stays on the B-rep side: 2D wire offsetting on self-intersecting input is
where it is weakest, and it has no place in a ~17k-polygon-per-layer loop.

---

## 5. Toolpath generation

**Shared front end:** all three operations consume the unioned net polygons of
a layer (polarity already applied). They differ only in what they emit.

| | Isolation (`pcb_isolation`) | Laser layer (`pcb_layer_cut`) | Drill (`pcb_drill`) |
|---|---|---|---|
| Input | copper layer | any layer (silk/mask/paste/copper) | Excellon |
| Geometry | offset out by `tool_radius + clearance` (via `cam2d::offset_closed_loop`), once per pass; tab and trim logic | contours of the net polygons; optional hatch fill for filled areas | point hits per tool |
| Extras | keep-clear around drills, pass ordering (inside→out), lead-ins | kerf width compensation, hatch angle/spacing, air assist | via vs component classes, spot/peck option |

Registration/origin comes from the `profile` layer bbox (centre or lower-left,
per machine setup); bottom-side layers get X-mirroring in the operation, so the
stored geometry stays in board coordinates.

**Decision — isolation is a 2.5D milling operation, V-bit included.**  An
earlier draft of this document claimed the CAM had no plunge/depth model and
that V-bit isolation would need one.  That was wrong: the milling side already
carries `stepdown_mm`, `plunge_feedrate_mm_per_min` and
`plan_stepdown_levels()` (`src/core/cam/cam_planning.h`), and the laser
operations simply pass `plunge_feedrate_mm_per_min: 0` because they never move
in Z.  So isolation needs **no new CAM machinery** — it is a 2.5D milling op on
the existing depth/plunge path.

What is genuinely tool-specific is the **cut width**:

| Tool | Effective cut width | Passes |
|---|---|---|
| Flat end mill ⌀ *d* | *d*, depth-independent | XY offsets outward by *d*/2 + clearance |
| **V-bit** (tip ⌀ *t*, included angle θ) | *t* + 2·*depth*·tan(θ/2) | **Z levels**: deeper cuts are wider, so successive Z steps clear more copper |

That means a V-bit isolation op takes the tool's tip diameter *and* included
angle (or an explicit per-depth width), derives the effective radius per Z
level, and uses the existing `plan_stepdown_levels` walk to sequence the
passes — the practical V-bit workflow, where depth *is* the width control.
A flat end mill stays a single-Z, multi-XY-offset pass.

Two further reuses fall out of the same reconnaissance: `pcb_drill` maps onto
the existing `drilling_generator` (`DrillCycle` with G81/G83 and peck depth)
rather than a new generator, and the tool library already carries plunging
feedrates per operation.

---

## 6. IPC surface (to be frozen in P2)

Following the existing CAM command/event naming and the `id`-matched reply
pattern:

| Command | Payload | Reply |
|---|---|---|
| `pcb_import_set` | `{ directory, files[]?, roles? }` | `pcb_import_set_result` (features, roles, diagnostics) |
| `pcb_layer_update` | `{ feature_id, role, mirror? }` | `document_state` |
| `pcb_layer_remove` | `{ feature_id }` | `document_state` |
| `pcb_isolation_create` | `{ layer_feature_id, tool, clearance, passes, tabs, keep_drills }` | `cam_operation_create_result` |
| `pcb_layer_cut_create` | `{ layer_feature_id, mode: contour\|fill, laser params }` | `cam_operation_create_result` |
| `pcb_drill_create` | `{ drill_feature_id, tool_map }` | `cam_operation_create_result` |

Every new message updates `protocol/schema/*.json`, `wiki/IPC-Protocol.md`, and
`wiki/AI-CAD-Command-Language.md` in the same change.

---

## 7. UI / UX

`pcb` workspace, three panes:

- **Left — layers:** imported files with resolved role, polarity, point/polygon
  counts, and per-file diagnostics badge. Role is overridable (the resolution is
  a guess, never a straightjacket).
- **Centre — board view:** fabrication preview (not a toolpath): layer
  visibility, top/bottom flip, board outline, drill marks.
- **Right — job:** process selection (isolation / laser / drill) with its
  parameters, then **Create operations**.

Hand-off: **Create operations** creates CAM operations and switches to the CAM
workspace (the same shape as the existing CAM → GRBL handoff). Floating panels,
i18n through `en.json`, theme tokens only, no ids in user-visible copy — per the
binding rules.

---

## 8. Testing

- **Parser units** (`cad_core_pcb_gerber_test`): golden snippets from the EDM
  board (format/units, aperture table, RoundRect macro arithmetic, one region,
  one `%LPC` silkscreen run, Edge_Cuts arcs) plus synthetic cases the board does
  not cover: inch, trailing-zero-omitted, `G74`, `%LPC` **in copper**, empty
  Excellon (NPTH), malformed/oversized input.
- **Geometry invariants** (`cad_core_pcb_polygonize_test`): area conservation
  through dark/clear boolean assembly, expected hole counts, offsets strictly
  monotone in pass index, no self-intersections, deterministic ordering.
- **Integration**: EDM board → operations → post; assert G-code invariants
  (no rapids through copper, beam off between passes, drills before/after
  isolation per setup).
- **Performance budget**: F_Cu (390 KB, ~16k strokes, two multi-thousand-vertex
  pours) parse + polygonize under 1 s; offsets + ordering under 1 s. Recorded as
  a test so it cannot regress silently.
- **Fixtures**: a trimmed slice is committed under
  `native/cad-core/tests/data/pcb/` (see its README for provenance and the
  exact trimming recipe): `copper_trimmed.gbr` — 35 KB keeping **all 71
  apertures**, 716 pad flashes, traces and one pour — plus the profile layer,
  a paste layer and both Excellon files.  The untrimmed plot stays local and is
  exercised by the same suites when `POLYSMITH_PCB_FIXTURE_DIR` points at it.

---

## 9. Phasing

| Phase | Deliverable | Gate |
|---|---|---|
| **P0** | This document + `wiki/Home.md` link | your review |
| **P1** | Gerber X1 + Excellon parsers (pure) + unit tests | tests green |
| **P2** | `pcb_layer` feature ✅ · `PcbRegionAttestation` + `pcb_*` IPC commands (next) | **approved**; feature verified by tests |
| **P2b** | thin `pcb` workspace UI: import + layer table + "create laser operation" | in-app verification (approved sequencing change) |
| **P3** | Laser `pcb_layer_cut` end to end (import → op → post → GRBL) | in-app verification |
| **P4** | `pcb_isolation` (2.5D milling op: cam2d offsets, `plan_stepdown_levels` for V-bit Z passes) + `pcb_drill` (existing `drilling_generator`) | in-app verification |
| **P5** | PCB workspace UI (layer table, board view, job pane, handoff) | in-app verification |
| **P6** | DXF layer + SOLID/HATCH polish in the existing importer (independent) | in-app verification |

P1 does not depend on the P2 review, which is why it can proceed in parallel.

---

## 10. Risks

- **Aperture macro coverage.** KiCad's `RoundRect` works; arbitrary user macros
  (nested expressions, primitives 2/5/6/21/22) may not. Unsupported macros must
  degrade per-aperture, not per-file.
- **Polygon count in the viewport.** Multi-thousand-vertex loops need a
  rendering path that does not tessellate into a mesh per recompute; this may
  interact with the STEP-import viewport work.
- **Arc tessellation tolerance** trades file size against isolation accuracy;
  it must be one documented constant, not per-call.
- **Untrusted input.** Gerber is user-supplied; the parser must be bounded
  (line length, coordinate magnitude, macro depth, total entities) and is a
  fuzz/crash surface. The CAD core's process boundary helps but is not a
  substitute.

## 11. References

- Ucamco *Gerber Layer Format Specification* (RS-274X + X2 attributes) — the
  normative source for the subset in §3.
- Excellon drill format (KiCad's plot output is the de-facto target here).
- [Clipper2](https://github.com/AngusJohnson/Clipper2) — Boost Software License
  1.0; *candidate* set-boolean engine, not yet a dependency (see §4).
- [FlatCAM isolation routing manual](https://flatcam.org/manual/iso.html) and
  `pcb2gcode` — reference implementations and test oracles for the isolation
  workflow (`pcb2gcode` itself is GPL: use as an oracle, not a dependency).

---

## 12. Implementation status

**P1a — Gerber command parser: done** (`src/core/pcb/gerber_parser.{h,cpp}`,
suite `cad_core_pcb_gerber_test`, 12 cases).

Text in, `GerberFile` out: format/units, zero-suppression modes, aperture table,
aperture-macro definitions **and** expression evaluation (`$n`, `+ - x /`,
parentheses, in-order `$n=…` assignments), the modal command stream
(move/line/arc/flash, regions, polarity, incremental mode), and the X2 role
attributes in both spellings (`%TF…%` and KiCad's `G04 #@! TF…*`).

Validated against the real board: F_Cu loads with all **71 apertures**,
**12 pours**, **716 pad flashes** and a ~16.5k-operation command stream with
`unsupported` **empty**; Edge_Cuts yields its 10 multi-quadrant arcs and 40
operations.  The fixture case is path/env gated, so the suite still passes
without the board present.

Two findings worth keeping:

- The command is spelled `ADD<code>` — reading the code straight after `AD`
  rejected every aperture definition.  The suite caught it (that is exactly the
  bug class the golden cases exist for).
- `G04 #@! TF.FileFunction,Profile,NP` is preserved **verbatim**; splitting it
  into role + modifiers (`Profile` / `NP`) belongs to the P2 role resolver, not
  the parser.

**P1b — polygonizer: done** (`src/core/pcb/gerber_polygonize.{h,cpp}`, suite
`cad_core_pcb_polygonize_test`, 10 cases).

Command stream → closed rings in millimetres, each tagged with the polarity in
effect: circle/rect/obround/polygon flashes, capsule strokes for round
apertures, exact flat-ended sweeps for rectangular ones, macro flashes expanded
into their primitives (circle / outline / vector line / rect / obround /
polygon), region contours, and tolerance-based arc sampling.  **No set
booleans** — by design (§4); clear rings are tagged, never dropped.

Validated against the real board: F_Cu polygonizes to **2017 rings with 0
skipped operations and 0 diagnostics**, which also corrected an assumption in
this document — the bulk of F_Cu's ~15.6k `D01` commands are *region outline
vertices* (two pours of several thousand points), not independent traces, so
ring count is far below command count.

**P1c — Excellon drills: done** (`src/core/pcb/excellon_parser.{h,cpp}`, suite
`cad_core_pcb_excellon_test`, 10 cases).

`M48` header with `FMAT,2` and `METRIC`/`INCH`, `TnC<diameter>` tool tables,
KiCad's `; #@! TF.`/`TA.` comments (plated vs non-plated, per-tool
`ViaDrill`/`ComponentDrill`), `G90`/`G91`, `G05`, `M71`/`M72`, `M30`, decimal
**and** fixed-point coordinates, and `R<n>[X…][Y…]` repeats.  Slots (`G85`) and
route mode are recorded and skipped; a hit whose tool was never defined is
counted (`hits_without_tool`) rather than dropped silently.

Against the committed fixtures: `pth.drl` yields **13 tools and 511 holes**
with no diagnostics, and `npth.drl` parses as a **valid empty program**
(zero holes is not an error).

Three defects the tests caught, all fixed before commit:

- **Repeat semantics.**  `R<n>` repeats the *previous* hole `n` more times,
  stepping incrementally by its `X`/`Y` — `R07X5000` is 8 holes 5 mm apart, per
  KiCad's own regression test.  The first implementation repeated in place and
  ignored the step, which would have drilled real holes in the wrong place and
  missed the pattern ones entirely.
- **Decimal files.**  A file that writes one explicit decimal point is a
  decimal file, so a whole-millimetre hole appears as `X10` — not as a
  fixed-point `0.01 mm`.  Point-less coordinates are now interpreted against
  the file's own convention (or the header's `; FORMAT={…}` hint) rather than
  per token.
- The fixtures contain **no** point-less coordinates, so only the synthetic
  cases covered this path; the real files would not have caught either bug.

**P2 (core slice) — the `pcb_layer` feature: done**
(`core/pcb/pcb_types.h`, `pcb_role.{h,cpp}`, `pcb_layer_io.{h,cpp}`,
`document/impl/pcb_commands.inc`, suite `cad_core_pcb_layer_test`, 10 cases).

`import_pcb_layer` / `update_pcb_layer` / `remove_pcb_layer` create and own a
`pcb_layer` feature: `kind = "pcb_layer"`, carrying `PcbLayerParameters`.  The
feature contributes **no B-rep geometry**, so the body compiler skips it exactly
as it skips a construction feature — verified, not assumed
(`build_feature_shape` returns an empty shape for unknown kinds).

**The one decision worth reviewing: geometry is re-derived, not embedded.**

| | STEP/IGES import (existing precedent) | `pcb_layer` |
|---|---|---|
| Stored | source path **plus a serialized shape snapshot** | source path **plus summary counts** |
| Part file | self-contained; source may be deleted | needs the Gerber set present |
| Missing source | still opens | `dependency_broken` + warning, summary kept |

A copper layer is ~2 k rings and tens of thousands of points; snapshotting that
into every save would add megabytes of data that re-derives in milliseconds.
The cost is that a moved or deleted Gerber file shows up as a broken dependency
(with the last known counts kept, so the timeline does not lie with zeroes).
If self-contained saves turn out to matter — a part mailed to someone without
the fabrication set — the parsed rings can be embedded as a sidecar without
changing this contract.

Role resolution is by **normalised keyword, never equality**, because the tiers
disagree on vocabulary: `Paste,Bot` (Gerber attribute) vs `SolderPaste,Bot`
(job file), `Legend` vs `Silkscreen`, `Profile` vs `Edge_Cuts`.  The file's own
attribute wins; the filename is the fallback; an explicit user role overrides
both and is recorded as such (`role_from_attributes = false`).  Solder-mask
layers carry a **negative** polarity — the file describes the openings — and
the polarity is assumed and labelled when the file does not state it.

The feature also keeps a bounded warning list (32 entries plus a suppressed
count), so one pathological file cannot grow the saved document without bound.

**Next — P2b:** the `pcb_*` IPC commands + protocol schema + TS types, then the
thin `pcb` workspace (import + layer table + "create laser operation") so this
can be exercised in the app; then `PcbRegionAttestation` and the CAM ops.

Committed-fixture coverage is in place: `copper_trimmed.gbr` polygonizes to
**1964 rings, 1215 macro-pad parts, 1 pour, 0 skipped, 0 diagnostics**, so both
suites now assert real-board behaviour without the local full plot.

