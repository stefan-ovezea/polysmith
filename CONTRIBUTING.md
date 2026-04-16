# Contributing to PolySmith

Thanks for contributing to PolySmith.

This project is intentionally built with strong architectural boundaries so that it remains understandable and maintainable over time.

## Core Principles

- Keep changes small and focused
- Prefer clarity over cleverness
- Preserve architecture boundaries
- Do not move CAD logic into the UI
- Do not bypass the IPC contract

## Before You Change Code

Please read:

- `README.md`
- `AGENTS.md`
- `docs/architecture/overview.md`
- `docs/architecture/ipc-protocol.md`
- `docs/decisions/0001-tech-stack.md`

## Development Workflow

### Run the desktop app

```bash
npm run dev
```

### Build the CAD core

```bash
npm run core:rebuild
```

## Change Scope

Good changes:

- one focused feature
- one bug fix
- one protocol improvement
- one documentation improvement

Avoid mixing:

- protocol changes
- UI cleanup
- architecture refactors
- unrelated formatting
- dependency changes

into one PR or one AI task.

## Coding Guidelines

- Write explicit code
- Keep functions readable
- Avoid hidden state
- Keep files focused
- Prefer simple data flow
- Add comments where intent is not obvious

## Architecture Rules

React owns:

- presentation
- user intent
- view state

The CAD core owns:

- CAD state
- document state
- geometry state
- feature history
- modeling behavior

Communication must go through the protocol layer.

## Protocol Changes

If you change message shapes or behavior, also update:

- `docs/architecture/ipc-protocol.md`
- `protocol/schema/*`

## AI-Assisted Development

If using Codex or other AI tools:

- follow `AGENTS.md`
- follow `docs/prompts/codex-rules.md`
- keep tasks narrow
- avoid broad autonomous refactors

## Testing

For non-trivial changes:

- add or update tests
- verify behavior manually if automated tests do not exist yet

## Pull Requests

Prefer PRs that are:

- small
- understandable
- single-purpose
- documented when architecture changes

## Philosophy

PolySmith should remain understandable by a human maintainer at all times.

AI is a tool to accelerate development, not a substitute for ownership.
