#!/usr/bin/env python3
"""Generate known synthetic pixels and simulated context/manual-box sequence; no third-party modules."""
import json
from pathlib import Path
import sys


def generate(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    # Object 0: exactly 60% candidate; object 1: bare. Not a real snow image.
    pixels = bytes(
        v for _y in range(100) for x in range(200) for v in ([230] * 3 if x < 60 else [60] * 3)
    )
    (out / "two.ppm").write_bytes(b"P6\n200 100\n255\n" + pixels)
    main = {"id": 0, "x": 0, "y": 0, "width": 100, "height": 100}
    aux = {"id": 1, "x": 100, "y": 0, "width": 100, "height": 100}
    frames = [
        {"angle": 1, "working": 1, "boxes": []},
        {"angle": 0, "working": 0, "boxes": []},
        {"angle": 0, "working": 1, "boxes": [main]},
        {"angle": 0, "working": 1, "valid": 0, "boxes": []},
        {"angle": 0, "working": 1, "boxes": [aux, main]},
    ]
    (out / "sequence.json").write_text(
        json.dumps(
            {"description": "SYNTHETIC pixels; SIMULATED context; MANUAL boxes", "frames": frames},
            indent=2,
        )
    )
    return out


if __name__ == "__main__":
    generate(sys.argv[1] if len(sys.argv) == 2 else "/tmp/ice_demo_input")
