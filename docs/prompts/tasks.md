# PolySmith Task Templates

Use these templates when asking AI to help with implementation.

## General Task Template

```text
You are helping develop PolySmith.

Project rules:
- React + TypeScript is for UI only
- Tauri manages desktop/native integration
- C++ native core owns CAD logic and state
- UI must not own CAD state
- Communication between UI and core must go through the IPC protocol
- Keep diffs minimal and reviewable
- Do not refactor unrelated code
- Do not introduce dependencies without justification
- Prefer explicit, readable code over abstraction

Please:
1. Explain the plan first
2. List files to change
3. Implement the smallest working version
4. Add comments only where intent is not obvious
5. Mention follow-up improvements separately
```

## Bug Fix Template

```text
Fix the following bug in PolySmith:

[describe bug]

Requirements:
- identify the likely cause first
- keep the fix minimal
- do not refactor unrelated code
- preserve architecture boundaries
- show changed files first
- explain any protocol changes explicitly
```

## New Feature Template

```text
Implement the following feature in PolySmith:

[describe feature]

Requirements:
- start with the smallest useful version
- preserve UI/core separation
- keep CAD logic in the native core
- keep the frontend focused on presentation and user intent
- update protocol docs if message shapes change
- avoid adding new dependencies unless necessary
```

## Protocol Change Template

```text
Update the PolySmith IPC protocol for the following change:

[describe protocol change]

Requirements:
- explain the protocol change clearly
- list all affected files
- update schema and documentation
- keep backward compatibility in mind
- do not mix unrelated refactors into this task
```
