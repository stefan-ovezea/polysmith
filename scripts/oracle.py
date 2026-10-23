#!/usr/bin/env python3
"""Analytic geometry checks ("oracles") against the built CAD core.

Most of the C++ suites assert that the code still does what it did yesterday.
These assert that it does what mathematics says it should: build a primitive,
export it, measure the exported mesh, compare with a closed form.

No build step, no third-party packages: it drives the `cad_core.exe` that is
already in `native/cad-core/build*`, exports an STL to a temp file and integrates
the mesh volume (sum of signed tetrahedra, exact for a closed triangle mesh).

    python scripts/oracle.py
    python scripts/oracle.py --tolerance 0.005

Exit code 0 when every case is within tolerance, 1 otherwise.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OCCT_BIN = REPO / "third_party/occt8-install/win64/vc14/bin"
CANDIDATES = (
    "native/cad-core/build/Release/cad_core.exe",
    "native/cad-core/build/cad_core.exe",
    "native/cad-core/build-release/Release/cad_core.exe",
)


def find_core(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            sys.exit(f"core not found: {path}")
        return path
    for candidate in CANDIDATES:
        path = REPO / candidate
        if path.is_file():
            return path
    sys.exit("cad_core.exe not found - build the core first (pnpm core:build)")


class CoreSession:
    """An interactive cad_core process: commands stay live so a case can read
    intermediate state (profile / face / edge ids) before the next command."""

    def __init__(self, core: Path):
        env = os.environ.copy()
        if OCCT_BIN.is_dir():
            env["PATH"] = f"{OCCT_BIN}{os.pathsep}{env.get('PATH', '')}"
        self.proc = subprocess.Popen(
            [str(core)], cwd=str(REPO), env=env,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
        )
        self._id = 0

    def send(self, type_: str, payload: dict) -> dict:
        self._id += 1
        cmd_id = f"c{self._id}"
        self.proc.stdin.write(
            json.dumps({"id": cmd_id, "type": type_, "payload": payload}) + "\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError(f"core exited before replying to {type_}")
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                continue
            if message.get("id") == cmd_id:
                if message.get("type") == "error":
                    detail = message.get("payload", {}).get("message", "unknown")
                    raise RuntimeError(f"{type_}: {detail}")
                return message

    def close(self) -> None:
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=30)
        except Exception:
            self.proc.kill()


def stl_volume(path: Path) -> float:
    """Signed volume of a closed triangle mesh (mm^3)."""
    data = path.read_bytes()
    if data[:5].lower() == b"solid" and b"facet" in data[:2000]:
        raise SystemExit(f"{path.name}: ASCII STL not supported by this reader")
    if len(data) < 84:
        raise SystemExit(f"{path.name}: not a binary STL ({len(data)} bytes)")
    count = struct.unpack_from("<I", data, 80)[0]
    expected = 84 + count * 50
    if len(data) < expected:
        raise SystemExit(f"{path.name}: truncated STL ({len(data)} < {expected})")
    total = 0.0
    for i in range(count):
        offset = 84 + i * 50
        # 12 float32: normal(3) v0(3) v1(3) v2(3); the normal at +0..11 is unused
        v = struct.unpack_from("<12f", data, offset)
        ax, ay, az = v[3], v[4], v[5]
        bx, by, bz = v[6], v[7], v[8]
        cx, cy, cz = v[9], v[10], v[11]
        total += (
            ax * (by * cz - bz * cy)
            - ay * (bx * cz - bz * cx)
            + az * (bx * cy - by * cx)
        ) / 6.0
    return abs(total)


def viewport(session: CoreSession) -> dict:
    return session.send("get_viewport_state", {})["payload"]


# ── Cases: each builds the shape and returns its expected volume ──────────

def case_box_40x30x20(session: CoreSession) -> float:
    session.send("add_box_feature", {"width": 40.0, "height": 30.0, "depth": 20.0})
    return 40.0 * 30.0 * 20.0


def case_cylinder_r10_h25(session: CoreSession) -> float:
    session.send("add_cylinder_feature", {"radius": 10.0, "height": 25.0})
    return math.pi * 10.0 ** 2 * 25.0


def case_box_thin(session: CoreSession) -> float:
    session.send("add_box_feature", {"width": 100.0, "height": 1.0, "depth": 1.0})
    return 100.0


def case_revolve(session: CoreSession) -> float:
    # Rectangle [20..30] x [0..30] revolved around the Y axis (x = 0).
    # Pappus: V = pi*h*((R+w)^2 - R^2) = pi*30*(30^2 - 20^2).
    session.send("start_sketch_on_plane", {"reference_id": "ref-plane-xy"})
    session.send("add_sketch_rectangle", {
        "start_x": 20.0, "start_y": 0.0, "end_x": 30.0, "end_y": 30.0,
        "is_construction": False,
    })
    session.send("add_sketch_line", {
        "start_x": 0.0, "start_y": -5.0, "end_x": 0.0, "end_y": 35.0,
        "is_construction": True,
    })
    vp = viewport(session)
    profile_id = vp["sketch_profiles"][0]["profile_id"]
    axis_id = next(line["line_id"] for line in vp["sketch_lines"]
                   if line.get("is_construction"))
    session.send("revolve_profile", {
        "profile_id": profile_id, "axis_entity_id": axis_id, "angle_degrees": 360,
    })
    return math.pi * 30.0 * (30.0 ** 2 - 20.0 ** 2)


def case_shell_box(session: CoreSession) -> float:
    # Cube 40^3 shelled 2 mm inward with one face removed: the opening face is
    # gone, so the cavity is (40-4)^2 * (40-2).
    a = 40.0
    t = 2.0
    session.send("add_box_feature", {"width": a, "height": a, "depth": a})
    vp = viewport(session)
    body_id = vp["solid_faces"][0]["owner_id"]
    session.send("create_shell", {"face_id": body_id + ":face:0", "thickness": t})
    inner = (a - 2 * t) * (a - 2 * t) * (a - t)
    return a * a * a - inner


def case_counterbore(session: CoreSession) -> float:
    # Cube 40^3, through-all counterbore from the top face: big D=10 to depth
    # 5, then D=6 through the rest. Removed = pi*5^2*5 + pi*3^2*(40-5).
    a = 40.0
    session.send("add_box_feature", {"width": a, "height": a, "depth": a})
    vp = viewport(session)
    body_id = vp["solid_faces"][0]["owner_id"]
    session.send("create_hole", {
        "face_id": body_id + ":face:0",
        "center_x": 20.0, "center_y": 40.0, "center_z": 20.0,
        "hole_type": "counterbore",
        "extent_type": "through_all",
        "diameter": 6.0,
        "counterbore_diameter": 10.0,
        "counterbore_depth": 5.0,
    })
    removed = math.pi * 5.0 ** 2 * 5.0 + math.pi * 3.0 ** 2 * (a - 5.0)
    return a * a * a - removed


def case_fillet_edge(session: CoreSession) -> float:
    # Box 40x30x20, fillet the longest straight edge (L=40) at r=3. The fillet
    # removes the corner square minus the quarter cylinder per unit length.
    w, h, d = 40.0, 30.0, 20.0
    r = 3.0
    session.send("add_box_feature", {"width": w, "height": h, "depth": d})
    vp = viewport(session)
    edge_id = max(vp["edges"], key=lambda e: e["length"])["id"]
    edge_len = max(e["length"] for e in vp["edges"])
    reply = session.send("create_fillet", {"edge_ids": [edge_id], "radius": r})
    fillet_id = next(f["feature_id"] for f in reply["payload"]["feature_history"]
                     if f["kind"] == "fillet")
    session.send("confirm_fillet", {"feature_id": fillet_id})
    removed = (r * r - math.pi * r * r / 4.0) * edge_len
    return w * h * d - removed


CASES = [
    {"name": "box 40x30x20", "tolerance": 1e-9, "script": case_box_40x30x20},
    {"name": "cylinder r=10 h=25", "tolerance": 0.01, "script": case_cylinder_r10_h25},
    {"name": "box 100x1x1 (thin)", "tolerance": 1e-9, "script": case_box_thin},
    {"name": "revolve rect @R20", "tolerance": 0.01, "script": case_revolve},
    {"name": "shell cube 40 t2", "tolerance": 0.01, "script": case_shell_box},
    {"name": "counterbore cube 40", "tolerance": 0.01, "script": case_counterbore},
    {"name": "fillet edge r3", "tolerance": 0.02, "script": case_fillet_edge},
]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core")
    parser.add_argument("--tolerance", type=float, default=None,
                        help="override every case's relative tolerance")
    parser.add_argument("--keep", action="store_true", help="keep the exported STLs")
    args = parser.parse_args()

    core = find_core(args.core)
    print(f"core: {core}\n")
    failures = 0
    out_dir = Path(tempfile.mkdtemp(prefix="polysmith-oracle-"))

    for case in CASES:
        stl = out_dir / (case["name"].replace(" ", "_").replace("=", "") + ".stl")
        session = CoreSession(core)
        try:
            session.send("create_document", {})
            expected = case["script"](session)
            session.send("export_document_stl", {"file_path": str(stl)})
        except Exception as exc:  # noqa: BLE001 - surface any build failure
            print(f"FAIL  {case['name']:22} build error: {exc}")
            failures += 1
            session.close()
            continue
        session.close()

        measured = stl_volume(stl)
        tolerance = args.tolerance if args.tolerance is not None else case["tolerance"]
        relative = abs(measured - expected) / expected if expected else float("inf")
        ok = relative <= tolerance
        failures += 0 if ok else 1
        print(
            f"{'ok  ' if ok else 'FAIL'}  {case['name']:22} "
            f"measured {measured:>14,.4f} mm3   expected {expected:>14,.4f}   "
            f"error {relative * 100:6.4f} %  (tol {tolerance * 100:.2f} %)"
        )
        if not args.keep:
            stl.unlink(missing_ok=True)

    if args.keep:
        print(f"\nSTLs kept in {out_dir}")
    else:
        try:
            out_dir.rmdir()
        except OSError:
            pass

    print(f"\n{len(CASES) - failures}/{len(CASES)} cases within tolerance")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
