# Refactoring plan — SolidCraft comparison, staged

Branch `feature/refactoring` (from `dev` @ `253d5be`). Sources: the SolidCraft
build report (`C:\SRC\SOLIDCRAFT-analysis.md`, local) and
[Lessons-From-SolidCraft.md](Lessons-From-SolidCraft.md). Effort is for one
developer; each stage is independently shippable and verifiable.

## Stage 0 — already done (not in this branch)

| what | where |
|---|---|
| document-state payload fix (deltas, binary buffers, marquee selection) | PR #92 (`253d5be`) |
| analytic oracle harness (box / cylinder / thin box) | `scripts/oracle.py` |
| bundled-theme revision fix | `src-tauri/src/app_config.rs` |

These address the two items the analysis ranks first (`§7.1` #1 payload, and the
first three oracle cases) and the theme bug found during testing. Everything
below is the remaining gap.

---

## Stage 1 — Trust: persistent naming (highest value)

The analysis and the lessons agree this is the single most valuable remaining
change (`analysis §5.3`, `§7.1` #2; `Lessons §8`). Today a reference is
`<body>:edge:<index>` re-resolved per replay, and a topology change can silently
re-bind to the wrong edge.

1. **`dependency_ambiguous` next to `dependency_broken`** — a re-bind that is not
   certain must be visible, not silent (`Lessons §7.2`).
2. **Construction provenance** alongside the index: `feature id + sketch entity
   id + role + face pair`, resolved provenance-first, geometric fingerprint
   second, index last (`Lessons §8`).
3. **Scope it**: fillet edges and face-based sketches first — the two that
   actually hurt. Do **not** chase sheet metal / surfaces.

**Effort:** 3–5 days for the two scoped features.
**First step:** `dependency_ambiguous` (small, unblocks the warn-don't-guess
behaviour), then fillet-edge provenance.

---

## Stage 2 — Never lose work (data safety)

`Lessons §1` and `§4`. `.polysmith` is currently the raw `DocumentState` payload
written straight over the destination, with no version field.

1. **Atomic save** — write `file.tmp`, `flush`, `rename`; keep `file.bak`
   (`Lessons §1`; one function + a test that kills the process mid-write).
2. **Recovery journal** — the core already speaks JSON commands, so log them to
   `<config>/journal/<doc-id>.jsonl` and replay on request (`Lessons §1` bonus).
3. **File-format version** — add `"format": "polysmith/1"`, one migration step
   per change, and a `tests/corpus/` of saved designs that must open (`Lessons §4`).

**Effort:** 3–4 days total.
**First step:** atomic save (highest risk of silent data loss, smallest change).

---

## Stage 3 — Prove geometry is right (verification)

`Lessons §2` and `§3`. The gap is *oracle* coverage, not test count (`Lessons`
"What not to copy"): 3,383 assertions pin current behaviour, so a consistently
wrong volume stays green.

1. **Extend `scripts/oracle.py`** beyond box/cylinder/thin box — revolve
   (Pappus 2π·R̄·A), shell of a box (outer − inner), counterbore
   (πR²h + πr²(d−h)), fillet on a straight edge ((r² − πr²/4)·length)
   (`Lessons §2`).
2. **IPC fuzz harness** — a Python generator (box → 8 random
   join/cut/fillet/shell/hole/pattern steps, 2.5 mm grid so tangencies are
   frequent), validate after every step (closed shells, positive finite volume,
   a cut never adds), 50 seeds, per-seed timeout (`Lessons §3`). Start with
   validity + volume direction — the part that matters for CAM, where a
   self-intersecting solid becomes a toolpath through the part.

**Effort:** 5–8 days.
**First step:** four more `oracle.py` cases; then the 50-seed fuzz script.

---

## Stage 4 — Refactor the core (code organisation)

The literal "refactoring" half (`analysis §5.5`, `§7.1` #4/#5; `Lessons §5`/`§6`).

1. **Break up the `.inc` monolith** — 497 fragments are 67 % of the core. Promote
   the largest (`cam_commands.inc`, `drawing_commands.inc`, `document.cpp`'s 72
   includes) into real `.cpp` units with headers, one per session (`analysis
   §7.1` #4). Low-risk, incremental.
2. **Make the architecture rules executable** — three greps in CI: no OCCT headers
   under `protocol/` or `core/document/`; no constraint/geometry math in the UI
   outside viewport draw code; no `make_document_state_event` from a handler that
   did not change the document (`Lessons §6`). Half a day.
3. **Command-registry metadata** — add id/parameter-type/description as handlers
   are touched, then generate the reference from it instead of the hand-maintained
   `wiki/IPC-Protocol.md` (`Lessons §5`). Payoff order: CAM scripting → agent
   surface → docs that cannot drift → command palette.

**Effort:** 1–2 weeks, incremental.
**First step:** the three CI greps (cheap, catches drift immediately), then the
first `.inc` promotion.

---

## Explicitly out of scope

- **A real test framework** (doctest/Catch2). `Lessons` says the ratio is healthy;
  the gap is *oracle* tests (Stage 3), not more test count.
- **The kernel** (`blend.rs`/`sew.rs`/`bridge.rs`) — written against `truck`,
  does not translate to OCCT, and OCCT already has those operations.
- **Browser build** — needs OCCT-on-wasm, its own project.
- **Icon pipeline / 24-language catalogues / PR-claim governance** — real work,
  not the bottleneck.

## Suggested order

**1 → 2 → 3 → 4.** Naming is what makes the product trustworthy; data safety is
what stops a crash costing a part; oracles and fuzz are what stop a silent-geometry
regression; the `.inc` breakup is hygiene that pays for itself in compile time and
IDE navigation but changes no behaviour.
