# Lessons from SolidCraft — what to adopt, what not to

Written 2026-10-21, after building SolidCraft 0.18.1 from source, running it,
driving it over its control channel and reading its kernel, solver, naming and
test code. Each item states what they do, the evidence, and the smallest useful
step here. Effort assumes one developer.

**Shipped the same day** (all verified: `tsc --noEmit` and `cargo check
--offline` clean, oracle 3/3):

| what | where |
|---|---|
| payload measurement harness | `scripts/measure_payload.py` |
| conformance harness for incremental state (+ self-test) | `scripts/ipc_conformance.py` |
| analytic oracle checks (box, cylinder, thin box) | `scripts/oracle.py` |
| finalized payload plan (phases C–F, measured) | [IPC-Payload-Plan.md](IPC-Payload-Plan.md) |
| ADR-0002 corrected to match the code | `wiki/ADR-0002-Core-Driven-Drag-Preview.md` |
| theme cache fix (bundled theme revisions) | `src-tauri/src/app_config.rs` |

Companion document: [IPC-Payload-Plan.md](IPC-Payload-Plan.md).

---

## 1. Never lose work — atomic save, autosave, recovery journal

**They do:** write a temp file, flush, rename over the original, keep `.bak`;
autosave only when something changed; offer recovery on the next launch; and
test it by killing the process 25 times mid-save.

**We do:** `DocumentManager::save_document_to_path`
(`native/cad-core/src/core/document/impl/session_cam_commands.inc`) writes
straight over the destination:

```cpp
std::ofstream stream(file_path);
stream << payload.dump(2);
```

A crash, power cut or full disk during that write truncates the user's only
copy. `autosave`, `.bak` and `journal` appear **zero** times across `native/`,
`apps/desktop-ui/src` and `scripts/`.

**Do:** write `file.tmp`, `flush`, `rename`; keep the previous version as
`file.bak`; autosave to a recovery folder every N minutes and before long
operations; offer to recover on startup.

**Bonus:** the core already speaks JSON commands, so "the commands since the
last save" *is* a recovery journal and a crash report. Log them to
`<config>/journal/<doc-id>.jsonl` and replay on request.

**Effort:** 2–3 days. **First step:** atomic save (one function, one test that
kills the process mid-write).

## 2. Oracle tests — test what is true, not what the code does

**They do:** `tests/oracle/` compares volume, area, centre of mass and bounding
box against values they did not compute — 83 cases, 62 analytic, 21 built by
FreeCAD through its public scripting API (nothing linked), with per-case
tolerances and a generated report (`docs/oracle.md`).

**We do:** 3,383 assertions, nearly all pinning existing behaviour. If extrude
computes a slightly wrong volume consistently, every test stays green.

**Do:** add a `tests/oracle/` family with closed-form expectations:

| feature | exact value |
|---|---|
| extruded rectangle/circle/L | area × height |
| revolve of a rectangle | Pappus: 2π·R̄·A |
| shell of a box | outer − inner |
| counterbore | πR²h + πr²(d−h) |
| fillet on a straight edge | (r² − πr²/4)·length |

**Effort:** 2–3 days for 15 cases. **Shipped 2026-10-21:** `scripts/oracle.py`
does the first three — a box (exact), a cylinder (0.04 % low, tessellation), and a
thin box — by exporting an STL and integrating the mesh volume, so the technique
is proven end to end. Add cases by extending its `CASES` table.

## 3. Fuzz through the IPC boundary

**They do:** a seeded generator builds realistic designs (box → 8 random
join/cut/fillet/shell/hole/pattern steps, snapped to a 2.5 mm grid so tangencies
are frequent), validates after every step (closed shells, positive finite
volume, a cut never adds), runs each seed in its own process with a timeout,
freezes per-seed goldens, and minimises failures.

**We do:** no fuzzing at all.

**Do:** the core is already a JSON process, so this can be Python — generate,
apply, check. Start with validity + volume direction (10 % of the fuzzer's
value, 5 % of the cost).

**Effort:** 3–5 days. **First step:** a script that runs 50 seeds and reports
invalid bodies; most important for CAM, where a silently self-intersecting
solid becomes a toolpath through the part.

## 4. Version the file format before the first user file breaks

**They do:** `"format": "solidcraft/N"`, one pending migration file per change,
the build assigns the number, a corpus of designs from older builds that must
still open *and evaluate*, and damaged variants (truncated, bit-flipped,
hostile numbers) that must error rather than crash.

**We do:** `.polysmith` is the raw `DocumentState` payload — **no version field**.
The first renamed or removed field makes older files unreadable with no path
forward.

**Do:** add `"format": "polysmith/1"`; keep one migration step per change; keep a
`tests/corpus/` of saved designs that must open.

**Effort:** 1–2 days. **First step:** the version field plus a corpus test.

## 5. Command registry — one surface for UI, scripts, tests, agents

**They do:** every user-visible action is a `CommandSpec` (id, label, tab, icon,
shortcut, parameter doc, enabled predicate, JSON runner); toolbar, palette,
dialogs, CLI, MCP and tests all call the same one. 404 registered commands.

**We do:** ~225 IPC command types with ad-hoc payloads; the contract lives in
prose in `wiki/IPC-Protocol.md` (100 KB, hand-maintained).

**Do:** not a rewrite. Add metadata as you touch commands (id, parameters with
types, description) and generate the reference from it. The payoff order:
CAM scripting → agent surface → docs that cannot drift → command palette.

**Effort:** 1–2 weeks incremental. **First step:** generate a table of the
existing `command.type == "..."` strings with their `read_*` payload keys.

## 6. Make the architecture rules executable

**They do:** `cargo xtask layers` fails the build if a lower layer imports a
higher one; `cargo xtask assets` fails if an asset has no attribution row.

**We do:** `CLAUDE.md` says "React must not own CAD state"; `ADR-0002` repeated
it; the drag path does it anyway (see §7).

**Do:** three greps in CI:

1. no OCCT headers (`#include <BRep`, `TopoDS_`, `gp_`) under `protocol/` or
   `core/document/`;
2. no constraint/geometry math in `apps/desktop-ui/src` outside
   `layout/viewport/` draw code;
3. every command handler that does not change the document must not emit
   `make_document_state_event`.

**Effort:** half a day. Rules that are not executable are aspirations.

## 7. One decision, one model (the ADR-0002 story)

**Where we are:** `wiki/ADR-0002-Core-Driven-Drag-Preview.md` decided on
core-driven per-move drag and explicitly rejected "a second code path". Two weeks
later the WASM preview landed
(`lib/planegcsBridge.ts`, `lib/speculativeSolve.ts`, called from `endpointDrag`,
`sketchMoveTool`, `snapResolution`), and the ADR was never updated. The document
now tells every new session the opposite of what the code does — and `CLAUDE.md`
tells agents to read the wiki first.

**The technical consequence:** the preview and the commit solve different
problems — H/V stripped during drag, ripple frozen to one hop, loose tolerance
(20 iters / 1e-4), and any constraint kind the bridge does not map is dropped by
`default: break` (`planegcsBridge.ts`, two sites). Only the dragged coordinate
crosses the boundary; every other point the preview moved is discarded and
re-derived by the core from the pre-drag state.

**Measured (2026-10-21):** a commit is not cheap either — `update_sketch_vertex`
on `Sketch3.polysmith` replies with **1,439,408 bytes** of document state (85 %
derived caches) plus a `get_viewport_state` refetch of **1,617,965 bytes**.
Keeping the preview local was the right call; the cost is the snap at release,
which is what item 3 below addresses.

**Do:**

1. Mark the ADR **Superseded** with the reason (the per-move commit also issues
   `get_session_state` + `get_viewport_state`, ~1.6 MB per move — measured), so
   the wiki stops contradicting the code.
2. Add `dependency_ambiguous` next to `dependency_broken` so a re-bind that is
   not certain is visible instead of silent.
3. Send the previewed coordinates on commit and let the core **verify and
   apply** (residual check, apply, warn past tolerance) instead of re-solving
   from the pre-drag state.
4. Make bridge coverage a test: enumerate the core's constraint kinds; each is
   either mapped or explicitly listed as "preview ignores this".

**Effort:** 1–2 days for all four.

## 8. Persistent naming — provenance, not indices

**They do:** the kernel reports what every output face and edge came from, and
names compose over named operands (`F5:side:l1`, `F7:blend:<edge>`,
`F4:top@F6.2`). When an operation cannot report history, the result gets **no
name** — "never guess".

**We do:** references are `<body>:edge:<index>` re-resolved per replay; on a
topology change the index may resolve to a different edge and the wiki calls that
"acceptable". The serialised evidence is in `wiki/Topological-Naming-Problem.md`.

**Do:** store construction provenance alongside the index
(feature id + sketch entity id + role + face pair), resolve by provenance first,
geometric fingerprint second, index last; warn when the match is ambiguous.
Do it for fillet edges and face-based sketches first; they are the two that hurt.

**Effort:** 3–5 days for those two; do not chase sheet metal / surfaces.

## 9. Generated defaults do not update (found while testing the theme)

**What happened:** a bundled theme was corrected, the app was rebuilt, the
colours did not change. `bootstrap_app_config`
(`apps/desktop-ui/src-tauri/src/app_config.rs`) writes a bundled theme **only if
the file does not exist**, and otherwise backfills only *missing* keys — so the
stored copy keeps its old values forever. The user-visible symptom is "I rebuilt
and nothing changed".

**Do:** add `"revision": N` to bundled theme files and overwrite the stored copy
when the bundled revision is newer (leave files that are not bundled alone, and
leave user edits after the same revision).

**Shipped 2026-10-21:** `bootstrap_app_config` now replaces a stored bundled
theme whose `revision` is behind the bundled one
(`apps/desktop-ui/src-tauri/src/app_config.rs`), and `solidcraft.json` carries
`"revision": 1`. The two stale copies under `%APPDATA%\polysmith\themes\` and
`%APPDATA%\Polysmith\themes\` were corrected by hand, since they predate the
guard.

**Effort:** half a day.

---

## What not to copy

- **The kernel work.** `blend.rs`, `sew.rs`, `bridge.rs` are written against
  `truck`; they do not translate to OCCT, and OCCT already has the operations.
- **The browser build.** They compile the whole app to wasm; that needs
  OCCT-on-wasm here, which is a project of its own.
- **Their test economics.** 5,644 tests suit a large agent-run project; the
  ratio here is already healthy — the gap is *oracle* tests, not more tests.
- **The icon pipeline and 24-language catalogues.** Real work, not the
  bottleneck.
- **Their PR-claim governance.** Built for many autonomous contributors.

## Open questions

- Does anything actually read `extrude_parameters.sketch_lines` /
  `profile_points`? The plan assumes not (see IPC-Payload-Plan §2); a grep says
  the preview uses `depth`, `mode`, `target_body_id`, `profile_ids`,
  `sketch_feature_id`. Confirm before deleting them from events.
- The viewport snapshot is rebuilt per document change (11 ms build). Is it
  built on a worker thread today, and does a stale revision ever reach the
  renderer?
---

## STEP import — how SolidCraft models it (studied 2026-11-10, not adopted)

Read from its docs (`C:\\SRC\\solidcraft\\docs\\step-import.md`), the document
model (`crates/doc/src/document.rs`, `Feature::Import`) and the import planner
(`crates/engine/src/cmd/file_import.rs`).

**They do:** one `Import` *base feature* on the timeline that **holds the STEP
text itself** as the design's B-rep (so the design reopens without the file),
and that feature builds **one body per solid** in the file:

- every import becomes a **component named after the file** (`bracket.step` →
  `bracket`), and the feature belongs to it, so imported bodies never land
  loose among the design's own;
- a STEP **assembly keeps its structure**: each product under the file's root
  becomes a nested component named after the product, at the file's placement;
- a part the file places **more than once becomes one component with an
  occurrence per placement** — the feature builds the part once and `skip`s the
  file's later copies (format 22, issue #1386), so repeats are instances, not
  duplicated geometry;
- bodies are named after the solid / product / instance (`names` option) and
  keep their body and face colours; `up` ("stand it up") turns the import;
- imported contents are **rigid** by default (assembly joints), released one by
  one with "Unlock Contents"; one import is **one undo step**.

**We do:** one `step_import` feature → **one body holding a compound of every
solid** (`core/geometry/import_commands.cpp`); `solid_count` is parsed and put
in the summary but never used to split. No components, no product tree, and
because a body's id IS its feature id, a single solid cannot be addressed
(no fillet on "the second solid", and `feature-N:face:M` may pick either lump).

**Two ways to match it here:**

1. *SolidCraft-shaped, for imports only* — keep one `step_import` feature
   holding the B-rep once and let the compiler emit one body per solid with ids
   `feature-12:solid-0`, `feature-12:solid-1`, … The `:face:` / `:edge:` owner
   parsers strip at the **last** known separator, and `body_feature_id()` maps
   back to the feature, so fillet / CAM / sketch-on-face keep working. Extrudes
   and fillets keep the plain `feature-N` contract, so the blast radius is the
   import path plus the owner-id parsers. Needs a multi-solid STEP fixture.
2. *Reuse operation grouping* — one feature per solid stamped with the same
   `operation_id` (one timeline row, N bodies). No body-id surgery, but a
   902-solid file becomes 902 `FeatureEntry`s each carrying a serialized shape
   snapshot — exactly the memory model SolidCraft's single-feature design
   avoids.

**Status: not adopted.** The reporter imported a backend PCB and got a very big
component tree plus a timeline full of the same "rigid group" feature; the
decision is to leave imports as they are for now and revisit after the extrude
work. Do not implement 1 or 2 without that decision.
