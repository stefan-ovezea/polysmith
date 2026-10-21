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


def run(core: Path, commands: list[dict]) -> dict[str, dict]:
    env = os.environ.copy()
    if OCCT_BIN.is_dir():
        env["PATH"] = f"{OCCT_BIN}{os.pathsep}{env.get('PATH', '')}"
    stdin = "".join(json.dumps(command) + "\n" for command in commands)
    completed = subprocess.run(
        [str(core)], cwd=str(REPO), env=env, input=stdin,
        capture_output=True, text=True, encoding="utf-8", timeout=600,
    )
    if completed.returncode != 0:
        sys.exit(f"core exited {completed.returncode}: {completed.stderr[-800:]}")
    replies = {}
    for line in completed.stdout.splitlines():
        if line.strip():
            message = json.loads(line)
            if "id" in message:
                replies[message["id"]] = message
    return replies


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


CASES = [
    {
        "name": "box 40x30x20",
        "build": [{"id": "b", "type": "add_box_feature",
                   "payload": {"width": 40.0, "height": 30.0, "depth": 20.0}}],
        "expected": 40.0 * 30.0 * 20.0,
        "tolerance": 1e-9,
    },
    {
        "name": "cylinder r=10 h=25",
        "build": [{"id": "b", "type": "add_cylinder_feature",
                   "payload": {"radius": 10.0, "height": 25.0}}],
        "expected": math.pi * 10.0 ** 2 * 25.0,
        "tolerance": 0.01,   # a tessellated cylinder under-reports its volume
    },
    {
        "name": "box 100x1x1 (thin)",
        "build": [{"id": "b", "type": "add_box_feature",
                   "payload": {"width": 100.0, "height": 1.0, "depth": 1.0}}],
        "expected": 100.0,
        "tolerance": 1e-9,
    },
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
        commands = [{"id": "create", "type": "create_document", "payload": {}}]
        commands += case["build"]
        commands += [
            {"id": "export", "type": "export_document_stl", "payload": {"file_path": str(stl)}},
        ]
        replies = run(core, commands)
        export = replies.get("export", {})
        if export.get("type") != "document_exported":
            print(f"FAIL  {case['name']:22} export failed: {export.get('payload')}")
            failures += 1
            continue
        measured = stl_volume(stl)
        expected = case["expected"]
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
