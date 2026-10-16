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

**P2b — IPC + the thin `pcb` workspace: done, USER-VERIFIED in-app.**
`pcb_import_layer` / `pcb_update_layer` / `pcb_remove_layer` plus the workspace
page (import, layer table with role override / contents / polarity / mirror /
warnings / remove).  The workspace emits no G-code and no toolpaths — its only
output is `pcb_layer` features, and the page says so.

Verified against the full EDM set (13 files, 1–2 s): **every file resolved its
role from its own attributes** (zero filename guesses), both mask rows came out
**Negative**, bottom-side layers defaulted to **Mirrored**, `Edge_Cuts` reported
the 99.9 × 99.9 mm board, `F_Cu` reported **2017 rings** — the same figure the
polygonizer suite pins — and `NPTH.drl` imported as 0 tools / 0 holes.

Two things the session taught, both recorded in the implementation log: the UI's
zod feature schema strips unknown keys, so `pcb_layer_parameters` must be
declared there or the table silently renders empty; and `AppTopBar`'s workspace
dispatcher used `showSlicerView()` as its fallback, so the new `pcb` view landed
on the Slicer page — now an exhaustive switch with a `never` assertion.

**P2c (identity half) — the layer geometry reference: done**
(`PcbLayerAttestation` in `cam_types.h` + protocol + suite
`cad_core_pcb_layer_attestation_test`, 5 cases).

An operation that cuts a layer references the **whole layer**, not individual
rings: both workflows (laser-cut every contour of a layer, isolate all of its
copper) consume the complete ring set, and referencing one thing instead of
thousands keeps the re-identification surface small.  The witness is the
feature id plus what the layer held (ring count + bounds); the rings are
re-derived from the source file on every recompute, per the project's
re-resolve rule.

The three outcomes are deliberately distinct, and each is tested:

| Situation | Result |
|---|---|
| Layer healthy | resolves, `changed = false` |
| Source re-plotted (different ring count/bounds) | **still resolves**, `changed = true` → the caller warns; the fix for a re-poured board is the new copper, not a vanished toolpath |
| Feature deleted, or source file gone/unreadable | does not resolve, with a reason naming the missing layer or file (never a crash, never a silent zero) |

Drill layers resolve on their **holes**, not rings (`pth.drl` → 511 hits across
13 tools), or a `pcb_drill` operation could never resolve at all.  The witness
survives a save/load round trip through the protocol.

**Generation is guarded, not silently empty.** `cam_generate` fails loudly
("Cutting an imported PCB layer is not wired into this generator yet.") when a
resolved PCB layer reaches a generator, so the next slice cannot accidentally
ship an operation that "succeeds" with no toolpath.  That guard comes out when
the laser branch lands.

**P2c (cutting half, core) — a layer cuts: done**
(`laser_generate.cpp` PCB branch + suite `cad_core_pcb_layer_cut_test`,
3 cases).

Every ring of a layer is an independent closed contour — a pad, a trace
outline, a pour boundary — so each ring becomes **its own group**: nesting
across rings is not a hole relationship, it is two separate cuts.  The layer's
coordinates are used verbatim as world XY with the cut at the operation's plane
offset; placing the board under the head is the machine's job today (pinning
the origin to the profile layer is a later refinement).

Proven against the committed profile layer (20 rings, kerf 0.2):

- **20 cut contours, one per ring** — counted as laser-on runs;
- the kerf pushes the cut **outside** the drawn outline, by no more than the
  kerf;
- no move strays beyond the layer by more than the kerf;
- a **drill-only layer** fails with *"…no contours to cut"* plus a warning
  naming the drill data — it must never "succeed" with an empty toolpath;
- a **deleted layer** degrades with *"…no longer in the document"*.

**Deliberately a parallel branch, not a shared helper.**  The face-outline path
above it duplicates ~40 lines of loop planning (orientation, centroid, kerf,
`plan_loop`).  A defect in the new branch therefore cannot reach the production
face and sketch paths.  The extraction into a shared helper is its own
change, with tests on both paths, to be proposed separately — approved in
advance by the user.

**§4.1 Region assembly — Clipper2 (done).**

The §4 question is settled by evidence rather than by preference: the committed
copper layer is **1,964 drawn primitives**, of which **1,215 are macro parts** —
KiCad's `RoundRect` pad expands to nine overlapping primitives, so the layer as
drawn cuts every such pad nine times, including lines that lie *inside* the pad.
That is wrong output, not merely slow output, and it forced the dependency the
doc had deferred.

`third_party/clipper2` (vendored in-tree, BSL-1.0, commit `f9c5eb6e`, version
2.0.1) now assembles drawn primitives into cuttable regions in
`core/pcb/pcb_region_assembly.{h,cpp}`:

- **union of dark draws, difference of clear draws, applied in file order**
  (`%LPD`/`%LPC` are not tags to carry around — they are operations);
- paths are normalized to one winding first, because Clipper2 unions under the
  NonZero fill rule and mixed windings would make an overlapping pad cancel
  itself instead of merging;
- a region with holes comes back as several rings — outer boundary plus one per
  hole, distinguished by signed area — which is exactly what a cutter needs.

`PcbLayerGeometry` therefore has two views: `rings` (what to cut) and
`drawn_rings` (the primitives as drawn, for diagnostics and for the tests that
pin macro expansion — `aperture_code`, `from_region` and `macro_primitive` are
only meaningful there).

Measured on the committed layers:

| | Drawn | Assembled |
|---|---|---|
| `copper_trimmed.gbr` | 1,964 | **573** |
| full-board `F_Cu` (local fixture) | 2,017 | **254** |
| `edge_cuts.gbr` | 20 | **8** |

Rapid travel on the copper layer fell from 3,533 mm to **1,833 mm** (36.7 → 19.0
board spans) on top of the ordering work, because the contour count itself is
what drives it.

**A real bug this uncovered:** the laser generator's duplicate-contour healing
pass (written for legacy *sketch* captures) pairs any two loops whose centroids
are within 0.05 mm and whose areas are within 25 %. The two boundaries of a thin
ribbon satisfy both, so it was **silently deleting real contours** on every
hairline layer — 8 assembled profile rings cut as 5. Layer loops now carry
`PlannedLoop::from_layer` and are never paired by that pass.

**There is no PCB workspace (decision, implemented).**

§1 chose a sixth top-level workspace `pcb`.  It has been **retired**, on the
user's call, because it had become a workspace whose only job was importing:

- **Import lives in CAD.**  File → *Import PCB…* (multi-select; Gerber *and*
  drill), alongside every other importer: one import path, one hierarchy, one
  undo/save story.  Multi-select because a job is a SET and a laser or mill board
  needs only a few of its files — importing the rest would just clutter the
  hierarchy.
- **The operation lives in CAM.**  A layer is an ordinary geometry source for a
  2D cut, chosen in the operation panel's **Geometry** block: a layer and a
  sketch scope answer the same question, so they sit together, with *None (model
  geometry)* handing the operation back to sketches.  The CAM "2D Cut" action
  also ensures a laser setup exists (a laser operation cannot join a mill
  setup) — the behaviour that used to live on the workspace's button.
- **The layer table became feature properties.**  What the table actually
  carried is metadata about the imported FILE, so it is now a right-click →
  **Properties** card on the feature: role *and where it came from* (the file's
  own attribute vs a filename guess), format, polarity, contents (contours, or
  tools/holes for drills), size, the mirror toggle, and the import warnings —
  the part that earns its keep, since that is how a negative plot or a guessed
  role gets caught.  Imported layers also gained their own **Imports** category in
  the hierarchy: a `pcb_layer` has no B-rep shape, so it used to fall through
  every category and appear only in the timeline.

The core did not change shape for this: `pcb_import_layer`,
`pcb_capture_layer_reference` (witness minted core-side) and the operation
update that accepts `layer_feature_id` are the commands the workspace used.

**Leads are off by default on a layer cut — and that is not cosmetic.**  A
contour shorter than **twice** the lead allowance is skipped, because the lead
cannot fit on it.  A PCB is full of small features: a 0.8 mm via ring is under
4 mm around, so the CAM default of 2 mm in + 2 mm out silently dropped vias and
mask openings (warned, but nothing on screen).  Layer cuts therefore start with
`lead_in_mm = 0` and `lead_out_mm = 0`, which is also what you want when the cut
line IS the file's own outline.  Pinned by `small_features_survive_without_leads`
in the cut suite: 2 mm leads cut 1 of 2 pads and report the skip; no leads cut
both.

**Measured on the real board** (`projects/EDM`; the polygonizer suite prints
these when that fixture directory is present):

| Layer | Drawn | Assembled | Notes |
|---|---|---|---|
| `F_Cu` | 2,017 | 254 | 0 skipped, 0 diagnostics |
| `B_Cu` | — | **122** | 0 skipped, 0 diagnostics |
| `B_Mask` / `F_Mask` | — | 134 / 262 | openings, not material |
| `In1_Cu` / `In2_Cu` | — | 180 / 92 | |
| `B_Silkscreen` | 2,199 | 208 | **1,493 strokes → 207 chains: 19 closed, 188 open**, widths 0.1–0.254 mm |
| `F_Silkscreen` | — | 792 | |
| `Edge_Cuts` | 20 | 8 | 4 closed chains in centre-line mode |

The silkscreen row is the one that matters for line art: KiCad draws its text
with a **stroked** font, so centre-line mode follows 188 open paths plus 19
closed component outlines — the Fusion "open chain" behaviour, on the real
board, with nothing approximated.

**Open chains (Fusion's "open chain") — PCB half done, sketch half planned.**

An open chain is geometry that simply *ends*: silkscreen text, a logo flourish,
an engraved line, a label.  Fusion lets you select one as an "open chain" and
cuts it as a path.  PolySmith needs the same, and both halves share one emitter
mechanism.

*Done (PCB layers).*  Stroke centre-lines are now chained into `PcbChain`s with a
`closed` flag (`PcbLayerGeometry::stroke_chains`); centre-line mode cuts closed
chains as loops and **open chains as open paths** — `PlannedLoop::is_open`, an
emitter branch that rapids to the first point, fires, walks the chain and stops
at the last.  No pierce search, no leads, no kerf, no closure, no tabs: an open
path has no seam to hang any of that off.  A single segment is a legitimate path
(silkscreen is full of them), so the three-sample minimum that guards the
closed-loop machinery does not apply.

*Planned (sketch geometry and text).*  Sketches already produce the right raw
material: sketch text is expanded into plain sketch lines tagged
`generated_by: "text:<id>"` (see `text_test.cpp`), so text is selectable geometry
rather than a special case.  What is missing is a reference kind and a pick:

- `GeometryReference` gains a **sketch-chain attestation**: the two endpoints,
  total length, segment count and the entity kinds along the chain.  TNP-safe in
  the same way as the profile witness — re-walk the sketch's live entities to
  find a path between those endpoints on every recompute; if the walk fails,
  degrade with `dependency_broken` plus a warning, never carry a stale index.
- The CAM workspace gets an **armed open-chain pick** (the Fusion gesture):
  click a sketch line and the core captures the chain through it, stopping at
  junctions and endpoints.  Selecting a text entity selects its glyph lines as
  chains — one open chain per glyph stroke.
- Operation semantics: an operation may mix closed profiles and open chains; the
  ordering pass already treats an open loop as area 0 with real endpoints, so
  nearest-neighbour and the Or-opt refinement cover both.

The emitter branch added for PCB strokes is exactly what the sketch half needs,
so this is not throwaway work.


`Edge_Cuts` is drawn with a 0.1 mm aperture, so assembly turns the outline into
0.1 mm-wide ribbons (8 rings = 4 ribbons × 2 sides) and regions mode traces
*both* sides of every ribbon.  A board outline means **"cut along this line"**,
so the interpretation is now a per-operation parameter,
`LaserCutParameters::layer_mode` (the user's call: it belongs on the 2D cut
operation, beside cut order, leads and kerf side — not on the layer):

- **`regions`** (default) — cut the assembled filled areas, kerf-offset.  Right
  for copper, mask and paste, where the file describes AREAS.
- **`centerline`** — follow each drawn stroke's centre line once, **no kerf
  offset and no leads**.  Right for the profile and silkscreen, where the
  aperture is a hairline.

Supporting work in the polygonizer: every stroke now records its centre-line
(`PcbLayerGeometry::stroke_paths`, with the aperture width), and those
centre-lines are **chained into closed loops** (`stroke_loops`) — a profile is
drawn as one stroke per side, so cutting them unchained would put a pierce mark
at every joint.  Chains that do not close are deliberately left out (cutting an
open path needs an open cut path the emitter does not have yet) and remain
available in `stroke_paths` for when silkscreen matters.

Measured on the committed profile layer: **4 cut contours** (board + 3 cut-outs,
one pass each) against **8** for the same layer in regions mode, and the cut
stays *inside* the drawn extents instead of a kerf-width outside them.  The PCB
workspace's **Laser cut** button seeds the mode from the layer's role — profile
and silkscreen start as centre-lines, copper/mask/paste as regions — and it
stays a normal operation setting afterwards.

**P2c (last mile) — create the cut from the layer table: done.**

`pcb_capture_layer_reference {feature_id}` mints the layer witness **in the
core** from the layer's own summary and replies with
`pcb_layer_attestation_result` — the same doctrine as
`cam_capture_face_reference`: the renderer names a row, the core decides what
identifies it.  The PCB workspace's per-row **Laser cut** button then reuses
the existing laser-cut action with that reference (no parallel operation
code path: the action gained an optional `pcbLayerFeatureId`), and switches to
CAM so the new operation's panel is in front of the user — the same
producer/consumer shape as the CAM → GRBL handoff.

So the chain is now complete end to end: **import a Gerber set → Laser cut on a
layer → CAM operation → post → GRBL workspace.**

**Next:** in-app verification of that chain, then `pcb_isolation` (2.5D milling
with V-bit width from tip ⌀ + included angle + depth, via
`cam2d::offset_closed_loop` + `plan_stepdown_levels`) and `pcb_drill` (existing
`drilling_generator`).  The shared-helper extraction for the laser loop
planning remains queued as its own test-backed change.

Committed-fixture coverage is in place: `copper_trimmed.gbr` polygonizes to
**1964 rings, 1215 macro-pad parts, 1 pour, 0 skipped, 0 diagnostics**, so both
suites now assert real-board behaviour without the local full plot.

