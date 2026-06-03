# PolySmith — Project Onboarding

When starting a new session in this project, read these wiki pages first to
understand the system. They are the canonical documentation.

## Reading order (first session)

1. **[Core-UI Design Principles](wiki/Core-UI-Design-Principles.md)** — **READ FIRST.** What belongs in core vs UI.
2. **[Architecture Overview](wiki/Architecture-Overview.md)** — UI / Tauri / C++ core layout
3. **[Contextual Modeling Workflow](wiki/Contextual-Modeling-Workflow.md)** — the binding UX pattern every feature follows
4. **[IPC Protocol](wiki/IPC-Protocol.md)** — how UI and core communicate
5. **[Topological Naming Problem](wiki/Topological-Naming-Problem.md)** — the project's mantra

## Return-session quick-ref

- **[Repository Map](wiki/Repository-Map.md)** — directory layout
- **[AI CAD Command Language](wiki/AI-CAD-Command-Language.md)** — IPC command reference for agents
- **[V1 Roadmap](wiki/V1-Roadmap.md)** — current priorities
- **[Implementation Log](wiki/Implementation-Log.md)** — what's shipped, including platform-specific build fixes

## Wiki Mirroring

The PolySmith wiki is mirrored to the GitHub wiki repository:

| Location | Description |
|---|---|
| `wiki/` | Canonical source — edit here first |
| `polysmith.wiki/` | GitHub wiki mirror (git submodule) |

The normal flow is **canonical → mirror**: changes originate in `wiki/` and
are propagated outward. Seldom, changes may originate in the mirror and need to
flow back to canonical. In either case, both locations must stay consistent.

### When editing wiki documentation

1. Make the change in `wiki/<file>.md` first.
2. Mirror the identical change to `polysmith.wiki/`.
3. Verify the mirror copy matches after the edit.

### When adding new wiki pages

1. Create the file in `wiki/<New-Page>.md`.
2. Add a link from `wiki/Home.md`.
3. Copy the new file to `polysmith.wiki/` and mirror the `Home.md` link update.

## Rules

- All documentation lives in `wiki/`.
- AGENTS.md at repo root is the binding instruction set — read it at session start.
- When adding new docs, create the file in `wiki/` and add a link from `wiki/Home.md`.
- Cross-reference wiki pages by their title-cased name without extension (e.g., `Architecture-Overview`).

## Debugging

- PolySmith is a **Tauri desktop app**. There is no F12 DevTools console.
- `console.log` and `stderr` output from the webview is captured and discarded
  by Tauri; it will never reach the developer.
- The app has a built-in **Logs panel** (opened via the toolbar). To emit
  messages there, use `addMessage("...")` from `useCadCoreStore` in App.tsx.
  The store hook is:
  ```ts
  const addMessage = useCadCoreStore((state) => state.addMessage);
  ```
- Structured `LogEntry` objects can be sent via `addLogEntry(entry)` for
  level/source/timestamp tagging. Both functions write to the same in-app
  log viewer.
- **Never use `console.log` for debugging.** It is invisible in Tauri and
  wastes the user's time. Wire `addMessage()` through the component tree
  instead or pass it through existing callback props.

## ⚠️ Active Priority (2026-06-03)

Per [Core-UI-Design-Principles](wiki/Core-UI-Design-Principles.md), the
following work is the top priority for the next session:

1. **Remove `resolve_draft_snap` IPC** — The C++ snap engine must not send
   per-frame snap candidates to the UI. Snap is interaction state, not
   document state. Remove the IPC command, its handler in `app.cpp`, and
   the `onResolveDraftSnap` call site in `ViewportPanel.tsx`. The
   `drag_snap_result` custom event must also be removed.

2. **Move drag back to UI** — Endpoint drag must be handled entirely in the
   TypeScript layer until mouse-up. The UI resolves snap locally from the
   static geometry in `viewport_state`, shows a preview without IPC
   round-trips, and sends a single `update_sketch_point` on mouse-up with
   the final snapped position. The `drag_sketch_point` and
   `drag_snap_result` IPC paths must be removed.

See removed pages: `Snap-Engine-Fix-Plan`, `Snap-System-CPP-Migration`,
`ADR-0002-Core-Driven-Drag-Preview`, `Sketch-Selection-Controls`.