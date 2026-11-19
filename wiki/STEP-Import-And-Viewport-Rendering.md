# STEP Import and Viewport Rendering

What an imported STEP assembly needs in order to render correctly, and what we
learned making the ESP32-P4C5 carrier (`ESP32-P4C5-Module_Carrier_V1.5.step`,
25 MB, 1,091 solids, 24,375 faces) look right. SolidCraft was the reference:
its renderer and its own STEP reader were read, and where our output differed
the cause was found before anything was changed.

## Import

**One body per solid.** A STEP assembly is a set of parts. Imported as one
compound it is a single row in the tree and one colour on screen. Each solid
becomes a feature/body named from the XCAF product structure (repeated
placements get ` #2`, ` #3`), and the whole import is one undo step.

**The hierarchy is the file's product structure**, not a body list: component
occurrences (`Board:1`, `U1:1 > …`) become document component nodes. See
[the Browser tree section](#the-browser-tree-is-the-files-product-structure).

**Colours live on sub-shape labels.** A face's or a small solid's style is
attached to *its own* label, not to the part. Asking for the colour of the
solid therefore returned nothing for 732 of 1,092 bodies, and the file's 52
colours came out as 14. The reader now walks every label once, keyed by shape,
and resolves each face: **face style → solid style → part style**. Per-face
lookup through `GetColor(shape)` instead costs 44 s on this board, because
each call walks the whole assembly.

**Nothing is silently dropped.** Faces outside any solid (126 here: legends,
markings, surface-modelled parts) become a `Surfaces` body. SolidCraft's own
reader (`crates/kernel/src/step_in/`) is written to the ISO-10303-21 standard
rather than a kernel translator, and its rule is that unreadable geometry is a
warning, never a silent drop. We import 24,375 of 24,375 faces; SolidCraft's
import of the same file reported 23,418.

**The file's black is the file's black.** Two rules used to repaint a near-black
body in the theme grey so black-painted shells would not vanish; they were a
workaround for the unlit face fill and they turned the PCB itself into a pale
slab. Removed — see [the section below](#the-colours-are-dull-the-importer-was-stripping-the-files-black)
for the measurement.

## Geometry and tessellation

**Normalize the normals.** The tessellator accumulates one unit normal per
incident triangle and never renormalized — the UI used to hide this by
recomputing normals. Used as-is, lighting scales with triangle density: bright,
flat, washed-out parts, and thin features melting into blobs. Renormalized
after accumulation; each face keeps its own vertices, so flat faces stay crisp
and curved ones stay smooth.

**Tessellation follows the body, not a constant.** `0.25 rad` angular (≈14°,
was 28.6°) and linear `1 % of the body diagonal`, clamped to `[0.02, 0.1] mm`.
A fixed 0.1 mm flattened a 0.4 mm castellation into a stepped ribbon. Lowering
the *linear* tolerance globally to 0.05 mm cost 26 MB of payload for nothing;
the angular term is what shapes small features.

## Rendering

**Follow SolidCraft's studio rig, do not approximate it.** `studio.rs` is the
whole model: three fixed neutral directionals — `(0.6, 0, 0.8)×3.0`,
`(-0.8, 0, 0.6)×1.0`, `(0, -0.6, 0.8)×1.5` — a BRDF, and a per-channel Reinhard
tone map. No ambient term, no cast shadows, no environment map, no indirect
bounces. Ambient + warm fill + PMREM room + shadow maps + ACES/neutral tone
mapping was a different renderer, and tuning it never converged. Three.js's
`ReinhardToneMapping` *is* their `c/(1+c)`.

**Per-face paint needs per-vertex colours.** One material colour per body
renders a connector's gold contacts, black body and white legend as one shade.
The core emits per-vertex linear RGB from the face styles; the UI renders with
`vertexColors`. Imported colours are diffuse paint, so `metalness 0` —
`0.1` silently subtracted 10 % of every imported colour.

**Engraved ink is invisible until it is lifted out of its recess.** The module
shield tops at `z = 3.25` and the file sinks its legend to `z = 3.24`: a 10 µm
engraving, which no renderer can show and which ties in the depth buffer so the
legend wins or loses per pixel (chopped glyphs at an angle, nothing from
above). The ink faces are lifted by `(dominant_top − ink_top) + 30 µm`, clamped
to `[0.03, 0.3] mm`, measured per body — a constant lift either ties with the
plate or leaves a deeper marking buried.

**A top-down view has no shading by construction.** Every visible normal is
parallel to the camera, so a directional light delivers one value to all
faces. Depth in that view can only come from occlusion or outlines; neither
SolidCraft nor we have an AO pass, which is why their top view and ours are
both flat — theirs is simply lighter because it renders black bodies grey.

## Cost (measured, ESP32-P4C5 carrier)

| | value |
|---|---|
| bodies | 1,092 |
| faces imported | 24,375 (all) |
| colours | 50, 23,583 faces |
| import | 12 s |
| viewport rebuild | 6 s |
| viewport payload | 62.9 MB |
| structural edges | 7,763 (≥ 1 mm, capped at 8,000) |

## Open items, in the order that would still change what you see

1. **USB-A / USB-C detail.** The housings are present (213 and 109 faces) and
   now grey, but the stamped ports and the USB-C tongue still do not read from
   the board. Next step is to isolate one housing body, dump its face colours
   and normals, and compare against SolidCraft's render of the same part —
   rather than another global shading change.
2. **Per-material PBR.** SolidCraft models metals as conductors and shields with
   clear coat (`pbr_appearance.rs`, `clear_coat.rs`); we still use one
   `MeshStandardMaterial` for every body. This is the last structural
   difference in their rendering.
3. **Project file size.** 99.5 % of a saved project is the ASCII B-rep
   snapshot (34.6 MB for this board). zlib is already vendored: compressing
   the snapshot on save should give single-digit MB with no behaviour change.
   Deferred until the document format is final.
4. **Storing the original file.** Keep the B-rep snapshot as the geometry of
   record (instant reload, immune to the STEP being moved or re-exported) and
   additionally keep the original STEP for provenance. Do not swap the
   snapshot for a file reference.
5. **Cut extrude / booleans on imported bodies.** A cylinder body can be added
   but a cut extrude does not make a hole — reported on this file, and
   SolidCraft reportedly fails the same case. Needs its own investigation;
   it is a modelling bug, not a rendering one. **Partly solved: see "Pick and
   boolean defects" below.**
6. **Face selection for sketch-on-face on imported bodies.** Reported: the main
   board's faces can be picked, other geometries cannot. Measured from the
   payload, every one of the 24,373 imported faces *does* ship pick triangles
   (none empty), and 17,660 of them are `planar`, so this is not a missing-proxy
   or a planarity problem. The next step is a click diagnostic: log which proxy
   the ray hits (or the nearest one and its distance) when a pick resolves to
   nothing, so the failing case names itself instead of being inferred. Likely
   candidates, in order: the proxy being too small to hit at the current zoom,
   the pick pass ordering a body mesh before its proxies, or the face being
   rejected later by the sketch-on-face gate rather than at pick time.
6. **Perspective camera.** SolidCraft's previews are perspective, ours
   orthographic; that is why text shears in a 3/4 view. Orbit, zoom-to-cursor
   and sketch framing all assume ortho, so this is real work and last.

## Pick and boolean defects: three measured causes

Three separate defects made "I cannot select this surface" and "the cut does
nothing" true. Each was measured on the ESP32-P4C5 carrier payload (24,373
faces, 7,763 structural edges) before being changed.

### 1. Face selection lost to a world-space edge tolerance (fixed)

`intersectViewportSceneTargets` tests edges BEFORE faces, and THREE.Line picking
compares `raycaster.params.Line.threshold` in **world** units. That threshold was
a fixed `1.2` - on a 50 mm board, not a line tolerance but the size of a small
part. Measured: **17,304 of 17,660 planar faces (98 %) had their centre within
1.2 mm of some edge**, so every click resolved to an edge. Every body was
affected (1,092 of 1,092); the "main board works" cases were the few faces with a
centimetre of clear interior. Hover therefore lit edges instead of surfaces, and
the double-click that starts a sketch on a face (`hit.kind === "face"`) never
fired.

The tolerance is now derived from the zoom - `EDGE_PICK_TOLERANCE_PX = 5`
screen pixels via `edgePickToleranceWorld(worldUnitsPerPixel)` - so the gesture
is the same at every zoom. At a PCB's working zoom that is about 0.28 mm, which
leaves roughly 10,000 of those faces reachable at their centre; the remainder are
sub-millimetre pads whose own outline is wider than the face, which no tolerance
can fix and which the selection filter serves.
Regression test: `apps/desktop-ui/src/layout/viewport/sceneTargetPicking.test.ts`.

### 2. A boolean consumed a primitive body and left it with no faces (fixed)

`append_cached_body_topology` skipped face enumeration for `box`/`cylinder`
bodies because the legacy primitive emitter supplies analytic
`front`/`top`/... faces. That emitter skips any feature a boolean consumed - so
after a cut the body had **zero faces**: it still rendered as a mesh, but had no
pick proxy, could not be selected, could not host a sketch and could not take a
second feature. Measured before the fix: a cylinder plus a cut reported
`1 body, 0 faces`. Faces are now enumerated whenever the primitive emitter will
not publish them.

### 3. A cylinder's faces were one radius away from its solid (fixed)

`BRepPrimAPI_MakeCylinder` centres the solid on its axis, so a cylinder body
occupies `(-r,-r,0)..(r,r,h)`. The analytic faces and the primitive were authored
for the corner-origin box layout (axis at `(r,r)`), so a sketch started on the
"top" face landed on the rim, one radius from the axis, and a cut through it
removed nothing. The CAM and drawing geometry - and their golden tests - are all
built around the centred placement, so the **viewport** was corrected, not the
solid: `cylinder_primitive_solid_feature_emit.inc` now publishes the faces and
the primitive at the body's real placement. Verified: cylinder + circle + cut now
yields `4 faces` including an `r=2.5` hole wall, and the mesh shows an `r2.5`
ring. No geometry moved, so no drawing or CAM golden changed.

### Still open: primitives after the first are laid out by the viewport only

The viewport lays primitives out in a row (`current_x_offset`, 30 mm per 20 mm
box) so a new body does not land inside the previous one, but the OCCT shape of
every primitive is built at the origin. The first primitive hides this (slot 0 is
the origin). Measured for a second box: body bbox centre `(10,10,2.5)`, its
analytic faces at `x = 30..50` - exactly one slot apart, so a cut on it misses
and an export puts it on top of the first body.

Baking the layout into the shape was tried and reverted: it relocates the solid
of every multi-primitive document and broke five CAM and drawing suites
(`adaptive_clearing`, `pocket_2d`, `contour_2d`, `drawing_projection`,
`drawing_annotation`), which is the measurement that says the layout is not
geometry. The fix is to give a primitive a **stored position**: a position field
in `BoxFeatureParameters`/`CylinderFeatureParameters` (schema + save + protocol),
written once at creation from the existing bodies' extents, used by the shape
builder, the viewport emitter and the move feature alike. That is a document
format change and needs a decision before it is made.

## "core did not respond to ..." is a clock, not a dead core

The UI gave every awaited command a flat 10 s (`sendCoreCommandAwaited`), and the
core handles commands strictly in arrival order. Importing this board's STEP
takes **11.9 s** (measured with the real core over the real protocol), so a short
command sent while an import runs waits for it: an `undo_end_group` queued behind
the import was answered **11.9 s** after it was sent — 100 ms after the import
finished — and the app had already failed it with "core did not respond to
undo_end_group". Fire-and-forget call sites (`void endUndoGroupRef.current()`)
turned that into an *unhandled rejection*.

Two changes:

- The timeout is now "the **core** went quiet": `shouldFailPendingCommand()`
  fails a command only after 10 s without any inbound message (the core narrates
  long work with log events), with a 300 s absolute cap for a core that keeps
  chatting but never answers. `noteCoreActivity()` runs on every core event,
  including progress logs; `subscribeCoreActivity()` lets other waiters use the
  same signal.
- `awaitDocumentChange`/`awaitViewportChange`/`awaitDocumentExport`/
  `awaitDocumentSaved` had the same flaw with 4 s deadlines and now use it too
  (10 s of silence, 30 s cap).

Separately, three handlers could answer **nothing at all**: `undo_begin_group`,
`undo_end_group`, `undo_abort_group`, `undo_abort_all_groups` and
`set_undo_limit` replied only when a document existed. `set_undo_limit` with no
document was silent even though the command had succeeded. They now always reply
(`reply_with_document_or_error`, error `NO_ACTIVE_DOCUMENT` when there is
nothing to report), and the two fire-and-forget undo-group call sites in
`ViewportPanel.tsx` catch and log instead of leaking an unhandled rejection.

## Imported colours were painted twice — the second time unlit (fixed)

The 4inch enclosure front (`ESP32-P4C5-Module_Enclosure_4inch_Front_V1.0.step`)
rendered as a flat, washed-out sheet next to SolidCraft's shaded grey part. The
payload named the cause exactly: **all 158 faces carry
`appearance_color: "#FFFFFF"`**, and the per-face fill material is a
`MeshBasicMaterial` — **unlit** — set to `opacity: 1` whenever a face has an
appearance colour (`applySolidFaceVisualState`). So every face was covered by an
opaque, unlit white card and the body's shading was never visible. A PCB hid the
same defect: its 23,583 coloured faces are tiny, so flat cards read as part
colours.

The colour was already on the body twice over: the STEP styles are baked into the
body mesh's **per-vertex colours** (`face_import_color_fallback.inc` reads the
same `face_colors` table) and into the lit body material
(`applyPrimitiveVisualState`). Measured on the board: 1,083 of 1,092 bodies carry
mesh colours with 44 distinct values, and the 9 that carry none have no coloured
faces either — so nothing loses its paint.

The face fill is now strictly a highlight layer (`opacity 0` idle, 0.08 hover,
0.5 selected). Shading comes back: rendered through the viewport's own material
model (studio rig, albedo × irradiance / π, Reinhard + sRGB) the enclosure's top
face lands at `#C7C7C7`, the two visible walls at `#A3A3A3` and `#828282`, i.e.
the same kind of separation SolidCraft's preview shows. Regression test:
`apps/desktop-ui/src/utils/viewport/visualState.test.ts`.

One consequence of the studio rig worth knowing: all three lamps have a +Z
component and there is no ambient, so any surface whose normal points +Y or −Z
receives nothing and renders black. That is SolidCraft's own rig (copied
verbatim, and the look you approved), not a side effect of this change — the
back of a panel is dark from behind.

## "The colours are dull": the importer was stripping the file's black

Once the unlit fill stopped painting over the model, the board looked washed out
next to SolidCraft: a light grey slab instead of black soldermask. That was not a
lighting problem — the **colours had been discarded before they reached the
renderer**. Two rules in `step_import_helpers.cpp` threw away a near-black body
colour so the body would fall back to the theme grey:

- one at solid level (`max_channel(solid_color) <= 16` → unstyled), and
- one when ≥90 % of a body's faces were black.

They existed to keep the black-painted USB housings visible — but the reason
those vanished was the opaque **unlit** face fill, not the colour. On this board
the rule hit the PCB itself: its solid style is `#0D0D0D` (max channel 13), so
the whole substrate was repainted in the theme's light grey and every part sat on
a pale slab. Measured after removing the rules: the board body comes back
`#0D0D0D`, and **all 24,373 faces carry a style (0 bodies without colours, was
9)**. The black shells read fine now because shading is back.

One material value was brought to SolidCraft's default in the same round: the
vertex-coloured body material's roughness 0.65 → **0.5** (their
`crates/render/src/scene.rs` uses metallic 0, roughness 0.5, ior 1.5, and three's
MeshStandardMaterial is the same GGX/Smith/Schlick model), so the directional
specular lobe has the same width as theirs.

## "The module lost its colours": the theme tinted the imported paint

This one is a two-layer disagreement, and it is worth reading before touching
either layer again.

An imported body's paint lives in **per-vertex colours** (`primitive.colors` from
the core's per-face STEP styles). `buildPrimitiveObject` therefore sets
`baseMaterial.color = 0xffffff`, because a material colour **multiplies** vertex
colours. But `applyPrimitiveVisualState` — the hover/selection pass that runs for
every body on every sync — set that colour to `appearanceColor ?? theme body
grey` in all three of its branches, silently overwriting the white.

In the dark themes `--color-cad-body` is `#45475a` (mocha): linear **0.06**. Every
imported surface was multiplied by 0.06, which is not a dim look but a
different-renderer look — a black PCB with gold contacts and a white shield came
out as one dark blue-grey part with no contrast left.

Measured on the same board (our viewport vs our own offline reference render of
the same dump, linear light): shield **0.037 → 0.44**, shells **0.036 → 0.4**,
gold contacts brown → gold. The predicted value for the shield from the rig alone
(albedo 0.6 × 4.2/π) is 0.80 → Reinhard 0.44, so the fix puts the live viewport
exactly where the reference renderer says it should be.

The rule now: a vertex-coloured body keeps white unless the user explicitly
assigns a body colour, and the highlight is carried by the edge colour and the
per-face fill, never by tinting the body (tinting would destroy the imported
paint on hover). Covered by four cases in `visualState.test.ts`.

The general lesson: **anything that sets `material.color` on a body has to know
whether the body's paint is vertex colours.** Both the geometry builder and the
visual-state pass set it, which is exactly how this survived.

## The dots on the model are vertex pick targets (now hidden when idle)

Every body vertex ships a small sphere (radius 0.25 mm, `SphereGeometry` scaled
0.25) so it can be picked for projection and snapping — the enclosure carries
**264** of them. They are pick targets, not geometry, but the idle state painted
them at opacity 0.95, so a part came out covered in a field of dots along its
slats, corners and posts, sitting on top of the very edges they mark. Idle is now
**opacity 0**: hovering reveals the marker in blue and a selection keeps it in
orange, so the target appears exactly when a click is about to use it. They stay
raycastable (only the opacity changed), so vertex picking, the CAM origin snap
and the WCS face pick behave as before. Regression test:
`apps/desktop-ui/src/utils/viewport/visualState.test.ts`.

## Confirmations: `window.confirm` is a broken shim in this app (fixed)

`tauri-plugin-dialog` installs an init script that **replaces `window.confirm`**
with `invoke("plugin:dialog|confirm")`. In plugin-dialog 2.7 there is no
`confirm` command any more — its `allow-confirm`/`allow-ask` permissions are
documented aliases of `message` — so every call failed with
`dialog.confirm not allowed. Command not found` and the rejection surfaced as an
unhandled rejection. The worse half: the shim returns a **Promise**, which is
always truthy, so `if (window.confirm(msg))` silently took the destructive
branch — deleting features and discarding sketches without asking.

`lib/confirmDialog.ts` wraps `ask()`, which goes through the `message` command
(that exists) and resolves to a real boolean; a dialog that cannot be shown
counts as "no". All four call sites now await it: dependent-feature delete, the
undo-history truncation, sketch-session cancel, and unlinking a linked body copy.

## "OCCT failed to create an extruded shape" was not OCCT (fixed)

That string came from `validate_occt_shape`, and only one path reaches it: the
parameters have **no profile ids**, `build_extrude_shape` returns a null shape on
purpose, and the validator blamed the kernel. The trigger is ordinary use:
`update_extrude_profiles` with an empty list empties a feature *by design* (the
panel's "0 selected" state), and the next depth-slider move re-validated it —
throwing at the slider. A polygon profile failed even earlier, with "Polygon
extrude requires at least three profile points".

`update_extrude_depth` now skips both validators when there is nothing to
extrude (`has_extrude_profile`), since neither is about the depth; re-selecting a
profile rebuilds the body as before. The remaining null-shape message now says
what to do: "Extrude produced no solid. The profile may be degenerate or
self-intersecting; try a simpler outline or a different depth".
Regression test: `test_depth_update_tolerates_an_emptied_extrude` in
`native/cad-core/tests/extrude_refresh_test.cpp`.

(Booleans that genuinely fail inside OCCT on a dense imported body — the case
reported at the same time — still fail, as they do in Fusion and SolidCraft; the
difference is that the app now reports it as a warning instead of a kernel
failure.)

## An imported file is ONE operation (the higher-level entity)

Fusion puts an imported assembly under one browser node, so "delete it", "hide
it", "find it" act on the whole thing. The provision for that already existed in
this codebase and was simply not used by the importer: `FeatureEntry::operation_id`
groups the features of one commit, `featureTimelineBlocks` folds a group into a
single timeline row, and `delete_feature` deletes the whole group as one undo
step. The STEP importer now stamps every body it creates — including the
loose-face surface body — with one operation id (the first body's feature id, the
project's convention).

Measured on the ESP32-P4C5 carrier (1,092 bodies):

- one distinct `operation_id` across all 1,092 body features,
- the timeline folds them into **one timeline entry**,
- deleting **any single body** removes all 1,092 in one step, and one undo brings
  all 1,092 back.

### One history entry, many component icons

Folding to one entry is right for HISTORY — the core's `timeline_cursor` counts
features, so the import is atomic and a cursor can sit before or after it, never
inside it — but it is wrong for the strip the user works in: a single icon for a
1,092-body board gives the timeline nothing to click. The strip therefore lays
out the file's **component occurrences inside that one entry**: the import block,
a fold chevron, then one icon per top-level occurrence (38 on the carrier), each
of which opens its children in place and shows its bodies as leaves (a part with
more than one body gets a chevron too, or only its first body would be
reachable). Nested runs are bracketed with a ring; the import's own chevron folds
the whole group back to the single entry.

Clicking a component opens it; clicking a body selects that body. "Find in
Timeline" from a face the user right-clicked in the viewport opens the chain of
occurrences holding that body (`componentAncestors`) and scrolls its icon into
view, so a part six levels inside the board is still one action away from its
geometry.

### Find in Timeline

Right-clicking a body (or one of its faces, edges or vertices) now offers
**Find in Timeline**: the viewport hands the owning feature id to the timeline,
which selects it and scrolls the row into view (`scrollIntoView`, added because a
real project hides the selection off-screen). That is Fusion's "Find in
Timeline" — from the geometry you see to its place in history.

### Deleting it

The same menu offers **Delete Body**, or **Delete N Bodies** when the body
belongs to a multi-body operation, and the confirmation names the count before
anything goes: "This deletes all N bodies of this operation (an imported assembly
is one operation). Continue?" — because the core's delete honours the group, and
a silent 1,092-body deletion is exactly the kind of surprise that has to be
announced.

Still to come, in the same shape: hide/show and material at the group level, and
"Save Copy As" / "Export" for the group. None of them needs a new document
concept — they are operations over `operation_id`.

## The Browser tree is the file's product structure

An imported assembly is not a list of parts. The file says `Board:1`,
`U1:1 > LLP-6_L2.0…:1 > …`, because a board places the same footprint a dozen
times and **the occurrence name is the only thing that tells the placements
apart**. Collapsing that into 1,092 rows — the "Imports" node with every body
under it — is what "it does not look like SolidCraft at all" meant: the hierarchy
IS the file.

**Core owns it as a document structure.** `core/document/component.h` defines
`ComponentNode { id, name, parent_id, body_ids }`, and `DocumentState::components`
holds them. Deliberately *provenance of the import*, not an assembly model: no
transforms, no mates, nothing that a future real assembly (component instances
posed with the vendored OndselSolver) would have to be bent around. A component
groups bodies (a body id is its feature id) and owns no geometry.

**The reader walks the XCAF product structure.** `assembly_from_xcaf` recurses
through `GetComponents` / `GetReferredShape` from each free shape that
`IsAssembly`, naming each node from the occurrence label and collecting solids
only at the *leaves* — an assembly occurrence's shape already contains its
children's solids, so collecting at every level would put every body under every
ancestor. Nodes that end up with no bodies, directly or through a descendant, are
pruned; a file we cannot map degrades to the flat list instead of losing bodies.

**Solids are matched by unlocated shape.** Located identity matches *nothing*:
the assembly compound and the component labels carry the same part TShapes but the
placement is attached differently (an instance location attribute versus a moved
referred shape). Keying on the unlocated shape and consuming one queued index per
placement reproduces the file's order exactly — measured: **1,091 of 1,092 solids
placed**, the single leftover being the loose-face `Surfaces` body, which is
correct (it belongs to no part).

Measured on the carrier:

| | |
|---|---|
| component occurrences kept | **536** (38 top level; the file has 415 NAUOs, 191 products) |
| deepest branch | `U4 (1):1 > ESP32P4…V1.2 v1:1 > U1 (1):1 > QFN-104_10x10mm_ESP32P4:1 > …` |
| `components` in the document payload | **66.9 KB** (document state 1,299 → 1,366 KB, +5 %) |
| viewport payload | unchanged (components carry no geometry) |
| import time | unchanged within noise (13.2 s) |

**The Browser renders it.** `componentHierarchy` nests the flat list by parent id
per import, a component row folds on click (an expanded default: the hierarchy is
what the user asked to see), its eye hides every body in the subtree in one
update, and bodies the file places in no component stay directly under the import
node. Component rows are grouping, not entities: no rename, no context menu, no
selection of their own — clicking folds.

**Format.** `components` is a plain document field: it round-trips through
save/load (covered by `cad_core_step_assembly_tree_test`) and needs no migration
because there is no released file format yet.






