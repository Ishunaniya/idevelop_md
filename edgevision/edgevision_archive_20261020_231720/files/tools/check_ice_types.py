"""Run C++ syntax/type checks using the delivery build's actual compiler flags."""

import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys

from check_ice_style import CPP_FILES, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    records = json.loads((args.build / "compile_commands.json").read_text())
    expected = {name for name in CPP_FILES if name.endswith(".cpp")}
    checked = []
    for record in records:
        source = Path(record["file"]).resolve()
        if not source.is_relative_to(ROOT):
            continue
        name = str(source.relative_to(ROOT))
        if name not in expected:
            continue
        command = record.get("arguments") or shlex.split(record["command"])
        command = list(command)
        output = command.index("-o")
        del command[output : output + 2]
        command.remove("-c")
        command.append("-fsyntax-only")
        subprocess.run(command, cwd=record["directory"], check=True)
        checked.append(name)
    missing = expected - set(checked)
    if missing:
        raise ValueError(f"Build does not cover the complete delivery scope: {sorted(missing)}")
    print(f"C++ syntax/type checks passed: {len(checked)} compilation entries")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
