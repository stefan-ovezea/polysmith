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

**Black is not a colour here.** A body whose dominant colour covers ≥ 90 % of
its faces and is at most `#101010` is treated as *unstyled* and takes the theme
body colour. SolidCraft's styled-item lookup only accepts a colour attached
directly to the body, so black-painted shells (USB housings) come back grey
there; OCCT's walk-up hands us the black style and the part disappears into a
near-black board. Mixed-colour bodies are excluded, which is what keeps the
module's grey plate plus black legend intact.

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
   it is a modelling bug, not a rendering one.
6. **Perspective camera.** SolidCraft's previews are perspective, ours
   orthographic; that is why text shears in a 3/4 view. Orbit, zoom-to-cursor
   and sketch framing all assume ortho, so this is real work and last.
