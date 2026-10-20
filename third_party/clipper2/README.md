# Clipper2 (vendored)

Integer polygon clipping — union, difference, intersection and offsetting — used
by the PCB/Gerber workspace to assemble a layer's drawn primitives into
non-overlapping region boundaries (`wiki/PCB-Workspace.md` §4).

**Upstream:** https://github.com/AngusJohnson/Clipper2
**Vendored commit:** `f9c5eb6e14a59f6f5d65fbfb3564519a561cf4fd` (reports version 2.0.1)
**Licence:** Boost Software License 1.0 — see `LICENSE` (permissive, and
compatible with this project's AGPL-3.0-or-later; the licence text ships here
because BSL-1.0 requires it in all copies).

Taken verbatim from `CPP/Clipper2Lib` (`include/` + `src/`), in-tree rather than
as a submodule — the same choice as `third_party/zlib` and
`third_party/libharu`, so a fresh clone needs no extra submodule step.

The CAD core compiles only what it uses (`clipper.engine.cpp`,
`clipper.offset.cpp`, `clipper.rectclip.cpp` — see `native/cad-core/CMakeLists.txt`);
`clipper.triangulation.cpp` and `clipper.export.h` (the C API) are kept so a
future need does not require re-vendoring.

## Why a dependency at all

A Gerber region set is *not* a set of independent outlines: aperture macros
expand to overlapping primitives (KiCad's `RoundRect` pad is nine), and `%LPC`
erases what earlier dark draws added. Emitting those as separate cut contours
traces the same pad nine times, including lines *inside* it — both wrong and
slow. Polygon union/difference with robust integer arithmetic is the piece that
turns drawn primitives into cuttable regions, and it is not something to
hand-roll.
