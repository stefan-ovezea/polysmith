# AGENTS.md

Guidelines for AI-assisted development (Codex, ChatGPT, etc.)

## Core Principles

- Do not make architectural changes without explicit approval.
- Keep changes minimal and scoped.
- Prefer clarity over cleverness.
- Do not introduce unnecessary abstractions.
- Do not introduce new dependencies without justification.

## Code Style

- Write explicit, readable code.
- Avoid magic behavior and hidden state.
- Prefer simple functions over complex class hierarchies.
- Keep files focused and small.

## Project Boundaries

- React (UI) must NOT own CAD state.
- CAD state, geometry, and modeling logic live ONLY in the native core.
- Communication between UI and core must go through the IPC protocol.

## IPC Rules

- Do not bypass the protocol layer.
- All communication must follow the defined schema.
- Always update protocol docs when changing messages.

## Workflow Expectations

When implementing a task:

1. Explain the plan before writing code
2. Show which files will be changed
3. Keep diffs small and reviewable
4. Avoid unrelated refactors
5. Add comments where intent is not obvious

## Testing

- Add tests for non-trivial logic
- Do not break existing behavior without explanation

## Forbidden Behaviors

- No large "vibe-coded" rewrites
- No silent refactoring across modules
- No mixing UI logic with CAD logic
- No bypassing architecture for speed

## Philosophy

PolySmith is built to be **understandable and maintainable by humans first**.

AI is a tool, not the owner of the codebase.
