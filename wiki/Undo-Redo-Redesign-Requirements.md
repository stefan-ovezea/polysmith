# Undo/Redo Redesign — Requirements & Design

> **Status:** Research complete (2026-09-17, 4 agents). Spec awaiting user
> approval of decisions D1–D10 before implementation on `feature/undo-redo`.
>
> **User request (verbatim):** "undo and redo is working sporadic. this
> tools was also done very long time ago and is behind not knowing about
> many new tools and stuff. I want again in deep analysis and a good plan
> to bring it to general accepted cad standard. right now has a mind of
> its own and some times work some times does not. … i want a full
> implementation. Probably need to be spitted between 2d and 3d
> implementation."

## Why a rewrite

The undo machinery is the oldest code in the repository (root commit
2026-04-16, whole-document snapshot stack) and has had only two
non-mechanical changes since (save/load integration 2026-04-28, a
mechanical file split 2026-06-08). Every tool since then was written
against it, producing 24 UI-side failure mechanisms (M1–M24) and 35
core-side findings (F1–F35). The full research reports are summarized in
`.deepseek/active-task.md` and the investigation sections below.

## Findings (condensed)

### Core (138 push sites, 56 files)

- **Undo/redo swap snapshots with zero follow-up work** (F8): no
  revision bump, no dependency refresh, no CAM cache invalidation, no
  selection prune. The restored snapshot carries an *older* revision —
  the revision clock goes backwards — and the revision-keyed CAM
  toolpath cache can serve a toolpath computed against an abandoned
  timeline branch (F22).
- **Extrude is the one panel that pollutes the undo stack** (F17/F21):
  all four extrude update commands push a full snapshot per call,
  multiplied by the number of live features in a multi-profile group
  (one slider tick = N pushes); its sibling `update_extrude_profiles`
  pushes nothing (F18). All other feature panels follow the
  create-pushes-once / update-pushes-zero "triangle" pattern — enforced
  only by comments (F16).
- **Two construction-plane preview commands clear the redo stack
  without pushing anything** (F23): nudging a preview after an undo
  destroys the redo branch.
- **Coverage holes:** `remove_sketch_projections` (Remove mode) can
  commit with zero undo entries (F14); DXF import's undoability is a
  side effect of a nested call (F19); `set_timeline_cursor` bumps the
  revision with a raw `+= 1` and no refresh passes (F29);
  `cam_operation_set_generated` persists user-visible state with no
  push and no bump (F20a).
- **Selection + timeline cursor live INSIDE the snapshot** (F2/F28):
  simultaneously not undoable (their commands push nothing) and
  overwritten by undo. ID counters live OUTSIDE the snapshot and never
  roll back (F5) — ids are never reused after undo (accepted, but
  documented).
- **"Nothing to undo" throws** and surfaces as `INVALID_COMMAND` —
  the same error code as an unknown command (F9/F35); the UI
  special-cases the message *string* in a control-flow loop.
- No depth limit, no step names, no saved-point tracking (F4).

### UI (24 failure mechanisms)

- `can_undo`/`can_redo` only exist in `session_state`, which is only
  emitted when explicitly requested — most sketch commands never
  request it, so the undo button/keystroke silently dead-ends until an
  unrelated command refreshes the flags (M17/M18/M19/M12). A
  `refreshSession` helper exists and is never called (M20).
- Every undo/redo invocation is fire-and-forget; the one correlated
  await mechanism exists but is used only by CAM (M1). The only
  "wait for document" primitive matches on object identity with a 4 s
  timeout (M2).
- Cancel-an-extrude can consume up to 10 undo steps in a loop (M5);
  one Escape can fire two cancel handlers (M9); a dimension draft
  commits as several commands (M22); typing in the extrude depth input
  creates one undo step per keystroke (M11); Ctrl+Z is dead while the
  auto-focused panel input holds focus (M10); no Ctrl+Y hotkey exists.

## The standard (distilled from Fusion 360, SOLIDWORKS, FreeCAD, Onshape)

See the research report for all 35 rules with citations. The ones that
drive this design:

1. **One user-intent = one undo step.** Never expose internal calls.
   FreeCAD's own defect: a line + its auto-constraints taking two
   undos (issue #19139).
2. **Group multi-command gestures explicitly** (transaction/group API);
   a drag stroke is one step committed on release; a dialog/task-panel
   session is one step — begin on open, commit on OK, abort on Cancel,
   and a cancelled operation leaves no trace.
3. **No history entry without a state change** (empty transactions are
   not recorded).
4. **Live previews never create entries and never clear redo.**
5. **Sketch session:** entering creates no step; exit commits;
   cancel rolls back the whole session. Onshape's model: per-action
   undo while open, whole-session step after close.
6. **Any new committed edit clears the redo stack** (branch conflicts).
   View/selection changes never do.
7. **Undo history is session-scoped, not saved to file** (PolySmith
   already does this); bounded by a configurable step limit; steps are
   **named** and shown in the UI ("Undo Add Line").
8. **Undo restores inputs and then recomputes** — downstream features
   and CAM toolpaths must be consistent after undo; undo must
   invalidate dependent toolpaths.
9. **CAM uses the same named-step system as modeling.**
10. **Undo/Redo enabled iff steps exist**, refreshed on every command;
    Ctrl+Z / Ctrl+Y (Shift+Z alias); a history dropdown that undoes the
    selected entry and everything above it, with confirmation.

## Decisions

- **D1 — Snapshot stack stays; undo/redo run the full refresh
  pipeline.** Keep the whole-document snapshot (a command/delta
  rewrite is a rewrite, not a fix), but undo/redo must run what every
  commit runs: fresh revision bump, dependency refresh, CAM cache
  `drop_stale`, selection prune, timeline-cursor normalization. The
  revision becomes monotonic; stale toolpaths become impossible.
- **D2 — Named, grouped steps.** The stack stores a step NAME with
  each snapshot ("Add Line", "Drag Point", "Extrude", "Trim Stroke").
  A lightweight group API lets one user intent span several commands
  (dimension draft session, sketch session, panel sessions): the
  snapshot is taken at group start, the name comes from the group.
- **D3 — Previews never push, never clear redo.** Fix extrude (adopt
  the create/update/confirm triangle like every other panel) and the
  two construction-plane previews (remove the redo-clear).
- **D4 — 2D sketch session semantics.** Enter = no step; exit commits;
  cancel rolls back the whole session as ONE step (FreeCAD's
  "Cancel sketch editing"). Inside the session, per-action granularity
  with gesture grouping: geometry + inferred constraints = one step
  (the #19139 defect fixed by design, not left to the linear stack).
- **D5 — 3D feature panels uniform.** Every feature panel follows
  create-pushes-once / update-mutates-silently / cancel-is-one-undo
  (or an explicit restore). The multi-profile extrude fan-out becomes
  one batch command. Cancel never loops undos.
- **D6 — Error contract.** Empty-stack undo/redo is a no-op that
  reports a proper error code (`EMPTY_UNDO_STACK`), never a throw
  surfacing as `INVALID_COMMAND`. Every state event carries fresh
  `can_undo`/`can_redo`.
- **D7 — UI correctness.** Undo/redo go through the correlated await
  path; `can_undo`/`can_redo` travel with every `document_state` (never
  stale); Ctrl+Y added; Ctrl+Z works even when a panel input has focus;
  single Escape path (no double handlers); preview panels tear down
  document-driven (feature gone → panel gone); in-flight gesture
  commits are revision-guarded.
- **D8 — History semantics.** Session-scoped (never saved — current
  behavior kept), configurable depth limit (default 30), named-step
  tooltips + history dropdown (undo N with confirmation). Timeline
  scrubbing stays a separate affordance but goes through the proper
  refresh passes. Saved-point tracking deferred.
- **D9 — CAM.** Same named-step system; `cam_operation_set_generated`
  stops persisting un-undoable state (generation becomes derived or
  pushes like any commit); undo invalidates cached toolpaths (via D1).
- **D10 — Tests.** A stage-isolated test program
  (`cad_core_undo_stages_test --stage N / --list`) isolates each
  building stage, plus per-suite additions for every fixed hole. Every
  fix gets a fail-before/pass-after test.

## Phases (each gated: build + all suites + tsc)

- **P0 — Core correctness (D1, D6, D10 skeleton).** Undo/redo refresh
  pipeline; empty-stack contract + error code; coverage holes fixed
  (`remove_sketch_projections`, `update_extrude_profiles`,
  construction-plane redo-clear, `set_timeline_cursor`,
  `cam_operation_set_generated`, `rename_feature` no-op guard); stage
  test program with stages for each.
- **P1 — 2D sketch sessions (D2 groups, D4).** Sketch session
  grouping: enter/exit/cancel semantics; dimension draft sessions as
  one step; geometry + inferred constraints as one step; Escape/cancel
  leaves no trace.
- **P2 — 3D feature panels (D3, D5).** Extrude normalized to the
  triangle; construction-plane previews fixed; all panel cancels
  become one undo / explicit restore; multi-profile batch.
- **P3 — UI correctness (D7).** Flags with every document state;
  awaited undo/redo; hotkeys (Ctrl+Y, global Ctrl+Z); single Escape
  path; document-driven panel teardown; revision-guarded commits;
  remove `undoUntilExtrudePreviewRemoved`.
- **P4 — History UX (D8).** Named steps in tooltips; history dropdown
  with multi-undo confirmation; configurable depth limit.
- **P5 — CAM pass (D9).** Verify/fix CAM participation; generated-state
  handling; toolpath invalidation end-to-end.

## Test program stages (planned)

1. Empty-stack contract (no throw; proper error code; flags).
2. Undo/redo refresh pipeline (revision monotonic, dependencies,
   selection prune).
3. Extrude panel session (create + N updates = ONE undo; cancel =
   one undo; redo intact until commit).
4. Construction-plane preview (no redo clear).
5. `remove_sketch_projections` Remove mode always one entry.
6. Timeline cursor scrub (refresh passes; no snapshot pollution).
7. 2D session: enter/exit/cancel rollback; dimension draft = one step.
8. Named steps + depth limit + multi-undo.
9. CAM: undo invalidates cached toolpaths; set_generated contract.
