# S11 — auto-constrain / fully-constrain: implementation plan

**Status:** planned, not started. **Estimate:** 3 d (plan); the first slice (detection) is ~½ d and is safe to land on its own.
**Parent:** [Sketch-Completion-Plan.md](Sketch-Completion-Plan.md) §S11 · [Sketch-Tools-Parity.md](Sketch-Tools-Parity.md) F5.

## Why this is its own plan

S11 reads like "wire up a command", but **there is no inference pass in the core**. The core has only per-constraint setters (`set_sketch_line_constraint` for horizontal/vertical, `equal_length`, `perpendicular`, `parallel`, `coincident`, `tangent`, `tangent_pair`, `symmetric`, `midpoint`, `collinear`). The snap inference the user sees while drawing is UI interaction state and never crosses the IPC boundary. So S11 is a new engine with a new acceptance loop, not a wrapper.

## Shape of the work

Four parts, in this order. **Parts 1 and 2 are the design; 3 and 4 are mechanical.**

### 1. Detection (pure, mutates nothing)

```cpp
struct AutoConstraintCandidate {
  std::string kind;                    // horizontal | vertical | equal_length |
                                       // coincident | concentric | parallel |
                                       // perpendicular
  std::vector<std::string> target_ids; // entity/vertex ids, in the setter's order
  double error = 0.0;                  // how far from exact, in mm or degrees
  std::string reason;                  // for the log: "line is 0.4° off horizontal"
};

std::vector<AutoConstraintCandidate> detect_auto_constraints(
    const SketchFeatureParameters& parameters,
    double tolerance_mm, double angle_tolerance_deg);
```

Rules, all deterministic (sort by id) so tests are stable:

| Candidate | Condition | Notes |
|---|---|---|
| `horizontal` / `vertical` | line angle within `angle_tolerance_deg` of an axis | skip a line that already has that constraint |
| `equal_length` | `|len_a − len_b| ≤ tolerance` | both lines must be free of any equal_length |
| `coincident` | two endpoints within `tolerance`, different vertices | reuse S5's untied-pair walk — it is the same condition |
| `concentric` | circle/arc centres within `tolerance` | skip if already concentric with that pair |
| `parallel` / `perpendicular` | direction angle within `angle_tolerance_deg` of 0°/90° | skip existing relations of that kind |

Dedupe against what the sketch already has, and never emit a candidate that the sketch already satisfies by construction (an exact rectangle from `add_sketch_rectangle` must yield **nothing**).

### 2. `try_add` — the acceptance rule

For each candidate, in order: add it, re-solve, keep it **only if `solver_dofs` drops**, otherwise roll that one candidate back.

- Keyed on the **solver's** number, not `count_sketch_dof` — the heuristic is what S3 wants to retire, and using it here would make S11 depend on a number that lies.
- Rollback granularity is one candidate: snapshot `constraints`/`line_relations`/`solver_dofs` before, restore on no-progress. Never abandon the whole run because one candidate was redundant.
- Stop early when `solver_dofs == 0`.

### 3. Dimensioning (only when `dimensions: true`)

One dimension per surviving DOF, ordered by locality: entity length → position of the first entity (x/y relative to the sketch origin) → offsets between entities. Every dimension is an expression-parameterised `SketchDimension`, so the user can drive it. Stop as soon as DOF is 0; never add a dimension that does not reduce DOF.

### 4. Undo, IPC, UI

- **One undo step** around the whole run: `push_undo_state()` before, `bump_geometry_revision()` after, like the S5 "Tie ends" batch.
- **Command**: `auto_constrain_sketch { feature_id?, tolerance?, angle_tolerance?, dimensions? }` → `feature_changed` (+ the full document when the run changed more than the partial-event contract allows; see D-003).
- **Read-only preview**: `preview_auto_constraints { ... }` returning the candidates and the projected DOF drop, so the UI can show "would add 6 constraints" before anything moves. This is also the AI/scripting surface.
- **Entry points**: a sketch-panel button ("Fully constrain"), the command, and a **single dismissible offer** when a sketch is finished with `solver_dofs > 20` — never an automatic run. Dismissal is remembered per sketch id for the session.

## Tests

1. **Detection, exact input** — a rectangle from `add_sketch_rectangle` yields no candidates (dedupe works).
2. **Detection, hand-drawn input** — four lines at 89.6°/90.4°/… and lengths within 0.3 mm yield exactly the expected kinds, by id, `profiles_match`-style (full set, not "at least one").
3. **`try_add` soundness** — a sketch that is already fully constrained gains nothing (`dofs` stays 0, no constraint added).
4. **`try_add` progress** — a loose quadrilateral reaches DOF 0; the count of added constraints is ≤ 8; one undo restores the original DOF and the exact original geometry.
5. **No silent change** — finishing an unconstrained sketch without clicking the offer changes nothing (compare the document byte-for-byte via `to_payload`).
6. **Both epsilon signs** on the angle rules: a line 0.4° above and 0.4° below horizontal must both be detected (the repo's tangency rule).
7. IPC: `preview_auto_constraints` returns the same candidate set as the C++ test, and `auto_constrain_sketch` is one undo step over the wire.

## Risks

- **Silently rewriting user geometry** is the worst failure mode of this command. Mitigations: `try_add` (a candidate that does not reduce DOF never exists), the preview command, and the offer never auto-running.
- **`point_on_circle` detection depends on D-008** (a constraint on a standalone point is stored but does not move it). Leave it out of the first version rather than emit a candidate that cannot be honoured.
- **S3 coupling**: the DOF badge and S11 will disagree while the emitter still uses `count_sketch_dof`. That is S3's problem, but S11 must not read the heuristic.
- **Tangent/arc cases** are where the solver is least predictable; exclude arcs from `equal_length` and tangent candidates in v1.

## First slice to land (½ d, no geometry mutation)

Detection + tests 1, 2 and 6, plus `preview_auto_constraints`. Nothing else — no `try_add`, no dimensions, no UI. This proves the rules and gives the UI something honest to show while the rest is built.

---

## Progress

| Slice | State |
|---|---|
| Detection (`detect_auto_constraints`) | **Done** — `1922dc9`. 4 C++ cases; dedupe reads the axis constraint where it actually lives (`SketchLine::constraint`, which is what D-009 pinned down). |
| Read-only preview (`preview_auto_constraints`) | **Done** — `790848e`. 3 IPC cases, including "the document is byte-identical before and after". |
| `try_add` apply pass (`auto_constrain_sketch`) | **Done** — 3 C++ cases + 2 IPC cases. A hand-drawn quadrilateral (four loose lines, 16 DOF) reaches **4 DOF** with 8 constraints added and 5 rejected. |
| Dimensioning pass | **Started.** Line-length dimensions run through the same try_add loop behind `dimensions: true`: a hand-drawn rectangle goes **16 → 1 DOF** with 3 dimensions kept. |
| UI entry points | **Button done.** "Fully constrain" in the sketch panel drives `auto_constrain_sketch { dimensions: true }` as one undo step. The dismissible finish-offer is not built. |

### What the apply pass established

- **The measurement is a direct solve, not `solver_dofs`.** That field is only populated when the refresh path happens to run the solver, and D-009 showed it can legitimately stay -1. `measure_solver_dofs()` builds and solves a throwaway `ConstraintSolver` for the candidate state, so the try_add rule is keyed on a real number.
- **Rejection restores the whole parameter block**, not just the constraint lists: a setter also rebuilds vertices, profiles and dimensions, so a partial restore would leave derived state from a rejected candidate behind.
- **A run that adds nothing pushes no undo step** and does not bump the revision; the manager snapshots the feature, and restores it when `added` is empty.
- **The reported DOF is published** into `solver_dofs`/`solver_ok` so the badge agrees with the constraints that were just added. `-1` (solve failed) leaves the field alone rather than inventing a number.
- **A rectangle bottoms out at 4 DOF, not 3**: closure plus two horizontal and two vertical sides already implies equal opposite sides, so the remainder is position (x, y) + width + height — **four** dimensions, not three. Fewer would mean a dimension had been invented.
- `concentric` candidates are **detected but skipped** by the apply pass: there is no `set_sketch_concentric_constraint` yet. They are reported in `skipped[]` rather than silently dropped.

### Dimensioning pass - what is and is not done

- Line-length dimensions run through the **same try_add loop** as the constraints, so a dimension the existing constraints already imply is rolled back rather than pinning a value the user never asked for. Verified on the hand-drawn rectangle: 3 survive, DOF 16 -> 1.
- **One DOF is left, and the acceptance is therefore not met yet.** Reaching 0 needs a *position* dimension (distance from the sketch origin to a vertex). There is no such call in the pass today, and `add_sketch_distance_dimension(first, second)` takes two entities, so the origin has to be represented as something the dimension can anchor to - that is the design question for the next session, not a one-line addition.
- Each length dimension drags a companion angle dimension with it (the interactive line tool does the same), which is why 3 dimensions remove more than 3 DOF. The test asserts the invariants (progress made, <= 8 dimensions, only the position DOF left), not the arithmetic.
- `dimensions` defaults to **false**, so an existing caller keeps the constraint-only behaviour.

### Still open

- Position dimension to reach DOF 0 (the acceptance in the plan).
- UI entry points: the sketch-panel button and the dismissible finish-offer at `solver_dofs > 20`.
- `concentric` apply (needs `set_sketch_concentric_constraint`).
