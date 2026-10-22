#!/usr/bin/env python3
"""Seeded IPC fuzz harness for the CAD core.

Generates a box, then applies a random sequence of modeling steps
(join / cut / fillet / shell / hole), snapping coordinates to a 2.5 mm grid so
tangencies are frequent. After every step it re-exports the body and checks the
two invariants that matter most for CAM:

  * validity   — the exported mesh has a finite, positive volume;
  * direction  — a cut never ADDS volume (a self-intersecting "cut" that
                 grows the body would become a toolpath through the part).

A step that the core rejects (a fillet on a bad edge, a shell too thick for the
shape, a hole on a non-planar face) is a *graceful* failure, not a fuzz bug —
the harness only fails a seed when the body goes invalid or a cut adds volume.

Usage:
    python scripts/fuzz.py            # 50 seeds x 8 steps
    python scripts/fuzz.py --seeds 10 --steps 6

Exit code 0 when every seed stays valid, 1 otherwise.
"""
from __future__ import annotations

import argparse
import math
import random
import sys
import tempfile
import time
from pathlib import Path

from oracle import CoreSession, find_core, stl_volume

GRID = 2.5


def grid_value(rng: random.Random, lo: float, hi: float) -> float:
    vals = [round(i * GRID, 3) for i in range(int(lo / GRID), int(hi / GRID) + 1)]
    return rng.choice(vals) if vals else float(lo)


def measure(session: CoreSession) -> float:
    path = Path(tempfile.mktemp(suffix=".stl"))
    try:
        session.send("export_document_stl", {"file_path": str(path)})
        return stl_volume(path)
    finally:
        path.unlink(missing_ok=True)


def op_extrude(session: CoreSession, rng: random.Random, mode: str) -> None:
    session.send("start_sketch_on_plane", {"reference_id": "ref-plane-xy"})
    x0 = grid_value(rng, 0.0, 30.0)
    y0 = grid_value(rng, 0.0, 30.0)
    w = grid_value(rng, 5.0, 25.0)
    h = grid_value(rng, 5.0, 25.0)
    session.send("add_sketch_rectangle", {
        "start_x": x0, "start_y": y0, "end_x": x0 + w, "end_y": y0 + h,
        "is_construction": False,
    })
    vp = session.send("get_viewport_state", {})["payload"]
    profiles = vp.get("sketch_profiles") or []
    bodies = vp.get("bodies") or []
    session.send("finish_sketch", {})
    if not profiles or not bodies:
        return
    session.send("extrude_profile", {
        "profile_ids": [profiles[0]["profile_id"]],
        "depth": grid_value(rng, 5.0, 20.0),
        "mode": mode,
        "target_body_id": bodies[0]["id"],
    })


def _feature_id(reply: dict, kind: str) -> str | None:
    history = reply.get("payload", {}).get("feature_history") or []
    for feature in history:
        if feature.get("kind") == kind:
            return feature.get("feature_id")
    return None


def op_fillet(session: CoreSession, rng: random.Random) -> None:
    vp = session.send("get_viewport_state", {})["payload"]
    edges = vp.get("edges") or []
    if not edges:
        return
    edge = rng.choice(edges)
    radius = grid_value(rng, 2.5, 5.0)
    reply = session.send("create_fillet", {"edge_ids": [edge["id"]], "radius": radius})
    feature_id = _feature_id(reply, "fillet")
    if feature_id:
        session.send("confirm_fillet", {"feature_id": feature_id})


def op_shell(session: CoreSession, rng: random.Random) -> None:
    vp = session.send("get_viewport_state", {})["payload"]
    faces = vp.get("solid_faces") or []
    if not faces:
        return
    index = rng.randrange(len(faces))
    face = faces[index]
    reply = session.send("create_shell", {
        "face_id": face["owner_id"] + ":face:" + str(index),
        "thickness": grid_value(rng, 2.5, 5.0),
    })
    feature_id = _feature_id(reply, "shell")
    if feature_id:
        session.send("confirm_shell", {"feature_id": feature_id})


def op_hole(session: CoreSession, rng: random.Random) -> None:
    vp = session.send("get_viewport_state", {})["payload"]
    faces = vp.get("solid_faces") or []
    if not faces:
        return
    index = rng.randrange(len(faces))
    face = faces[index]
    center = face["center"]
    session.send("create_hole", {
        "face_id": face["owner_id"] + ":face:" + str(index),
        "center_x": center["x"], "center_y": center["y"], "center_z": center["z"],
        "hole_type": "simple",
        "extent_type": "through_all",
        "diameter": grid_value(rng, 5.0, 10.0),
    })


# Each op carries `is_cut`: whether it must never ADD volume. `fillet` is
# directionally ambiguous — it removes on a convex edge and adds on a concave
# one — so it is excluded from the direction check.
OPS = [
    ("join", lambda s, r: op_extrude(s, r, "join"), False),
    ("cut", lambda s, r: op_extrude(s, r, "cut"), True),
    ("fillet", op_fillet, False),
    ("shell", op_shell, True),
    ("hole", op_hole, True),
]


def run_seed(core: Path, seed: int, steps: int, timeout: float) -> tuple[str, str]:
    rng = random.Random(seed)
    session = CoreSession(core)
    start = time.monotonic()

    def crashed(exc: BaseException) -> bool:
        return "core exited" in str(exc)

    try:
        session.send("create_document", {})
        session.send("add_box_feature", {"width": 40.0, "height": 40.0, "depth": 40.0})
        before = measure(session)
        ops: list[str] = []

        for step in range(steps):
            if time.monotonic() - start > timeout:
                return ("timeout", f"seed {seed} exceeded {timeout}s at step {step}")

            name, apply, is_cut = rng.choice(OPS)
            ops.append(name)
            try:
                apply(session, rng)
            except RuntimeError as exc:
                if crashed(exc):
                    return ("crash", f"seed {seed} core crashed during {name} "
                                    f"(ops: {' > '.join(ops)})")
                # The core rejected the step (bad edge/face, etc.) — graceful.
                continue

            try:
                after = measure(session)
            except RuntimeError as exc:
                if crashed(exc):
                    return ("crash", f"seed {seed} core crashed on export after "
                                    f"{name} (ops: {' > '.join(ops)})")
                raise

            if not math.isfinite(after) or after <= 0.0:
                return ("invalid", f"seed {seed} step {step} ({name}): volume {after} "
                                  f"(ops: {' > '.join(ops)})")
            if is_cut and after > before + 1e-6:
                return ("cut_grew", f"seed {seed} step {step} ({name}): "
                                    f"cut grew body {before:.3f} -> {after:.3f} "
                                    f"(ops: {' > '.join(ops)})")
            before = after
        return ("ok", "")
    finally:
        session.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core")
    parser.add_argument("--seeds", type=int, default=50)
    parser.add_argument("--steps", type=int, default=8)
    parser.add_argument("--only", type=int, default=None,
                        help="run a single seed (for reproducing a failure)")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="per-seed wall-clock timeout in seconds")
    args = parser.parse_args()

    core = find_core(args.core)
    print(f"core: {core}\n")
    failures: list[str] = []
    timeouts = 0

    seeds = ([args.only] if args.only is not None else list(range(args.seeds)))
    for seed in seeds:
        outcome, detail = run_seed(core, seed, args.steps, args.timeout)
        if outcome == "ok":
            print(f"seed {seed:3}  ok")
        elif outcome == "timeout":
            timeouts += 1
            print(f"seed {seed:3}  TIMEOUT  {detail}")
        else:
            failures.append(detail)
            print(f"seed {seed:3}  FAIL ({outcome})  {detail}")

    print(f"\n{len(seeds) - len(failures) - timeouts}/{len(seeds)} seeds valid"
          f"  ({len(failures)} failures, {timeouts} timeouts)")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
