"""Check formatting and whitespace for the winch-ice delivery scope."""

import argparse
import re
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FORMAT_FILES = [
    "include/evs/winch_ice.h",
    "include/evs/rknn_backend.h",
    "src/backend/rknn/detection_rows.h",
    "include/evs/testing/ice_replay.h",
    "src/core/monotonic_clock.cpp",
    "src/core/config_internal.h",
    "src/tasks/winch_ice/winch_ice_plugin.cpp",
    "src/tasks/winch_ice/winch_ice_plugin.h",
    "src/tasks/winch_ice/ice_measurement.cpp",
    "src/tasks/winch_ice/ice_measurement.h",
    "src/tasks/winch_ice/winch_ice_api.cpp",
    "tests/test_ice.cpp",
    "tests/ice_test_support.h",
    "tests/test_ice_allocation.cpp",
    "tools/ice_replay/main.cpp",
    "tools/ice_replay/replay_io.h",
    "tools/ice_replay/host_gate.h",
    "tools/winch_ice_demo/main.cpp",
]
# Existing shared files retain their surrounding style. Check their whitespace
# without reformatting unrelated code; keep them in the compiler-check scope.
SHARED_FILES = [
    "include/evs/function_plugin.h",
    "include/evs/task_config.h",
    "include/evs/vision_task.h",
    "include/evs/status.h",
    "include/evs/log.h",
    "src/core/config.cpp",
    "src/core/log.cpp",
    "src/vision_task.cpp",
    "tests/test_core.cpp",
]
CPP_FILES = FORMAT_FILES + SHARED_FILES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--shared-changes", action="store_true")
    args = parser.parse_args()
    if args.shared_changes:
        for name in SHARED_FILES:
            diff = subprocess.check_output(
                ["git", "diff", "--unified=0", "HEAD", "--", name], cwd=ROOT, text=True
            )
            ranges = []
            for start, count in re.findall(r"^@@ .*? \+(\d+)(?:,(\d+))? @@", diff, re.M):
                length = int(count) if count else 1
                if length:
                    ranges.append(f"--lines={start}:{int(start) + length - 1}")
            if ranges:
                subprocess.run(
                    [args.clang_format, "--dry-run", "--Werror", *ranges, name],
                    cwd=ROOT,
                    check=True,
                )
    subprocess.run(
        [args.clang_format, "--dry-run", "--Werror", *FORMAT_FILES], cwd=ROOT, check=True
    )
    paths = [ROOT / name for name in CPP_FILES]
    paths += list((ROOT / "tests/fixtures").rglob("*.py"))
    paths += [
        ROOT / "tests/test_ice_cli.py",
        ROOT / "tests/test_camera_cmake.py",
        ROOT / "tools/check_ice_types.py",
        Path(__file__),
    ]
    for path in paths:
        data = path.read_text()
        if not data.endswith("\n"):
            raise ValueError(f"{path}: missing final newline")
        for number, line in enumerate(data.splitlines(), 1):
            if line != line.rstrip() or "\t" in line:
                raise ValueError(f"{path}:{number}: trailing whitespace or tab")
    if args.shared_changes:
        print("Shared C++ changed-line formatting checked against HEAD")
    print(
        f"Style checks passed: {len(FORMAT_FILES)} C++ files formatted; "
        f"{len(CPP_FILES)} C++ files and scoped Python files checked for whitespace"
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
