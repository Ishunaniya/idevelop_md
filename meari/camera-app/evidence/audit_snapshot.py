#!/usr/bin/env python3
"""Read-only source inventory; write reproducible audit evidence to --output."""
import argparse
import csv
import hashlib
import json
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.source.resolve()
    out = args.output.resolve()
    if out == root or root in out.parents:
        parser.error("Output must be outside the source tree")
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    modules = defaultdict(lambda: {"files": 0, "source_files": 0, "source_lines": 0})
    extensions = Counter()
    missing = []
    shell_checks = []
    sdk_files = 0
    sdk_lines = 0
    source_suffixes = {".c", ".h", ".cpp"}
    for path in sorted(root.rglob("*")):
        if not path.is_file() or ".git" in path.relative_to(root).parts:
            continue
        relative = path.relative_to(root).as_posix()
        data = path.read_bytes()
        # A final unterminated line counts; only LF delimits physical lines.
        lines = data.count(b"\n") + int(bool(data) and not data.endswith(b"\n"))
        source = path.suffix in source_suffixes
        rows.append([relative, len(data), lines, hashlib.sha256(data).hexdigest(), int(source)])
        group = modules[relative.split("/")[0]]
        group["files"] += 1
        if source:
            group["source_files"] += 1
            group["source_lines"] += lines
            if relative.startswith("ipc_app/") and any(
                    part.startswith("tuya_sdk_") or part == "tuya_gw_sdk"
                    for part in path.relative_to(root).parts):
                sdk_files += 1
                sdk_lines += lines
        extensions[path.suffix or "<none>"] += 1
        if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            for number, line in enumerate(data.decode("utf-8", errors="replace").splitlines(), 1):
                active = line.split("#", 1)[0]
                for match in re.finditer(r"\$\{PROJECT_TOP_DIR\}/([^\s)\";]+)", active):
                    target = match.group(1)
                    if "$" not in target and not (root / target).exists():
                        missing.append([relative, number, target])
        if path.suffix == ".sh":
            result = subprocess.run(["bash", "-n", str(path)], capture_output=True, text=True)
            shell_checks.append({"path": relative, "exit_code": result.returncode,
                                 "stderr": result.stderr.strip()})
    with (out / "source-inventory.tsv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(["path", "bytes", "physical_lines", "sha256", "c_h_cpp"])
        writer.writerows(rows)
    with (out / "missing-cmake-paths.tsv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(["cmake_file", "line", "missing_literal_path"])
        writer.writerows(missing)
    capa = (root / "pps_device/pps_device_capa.c").read_text()
    plugins = []
    for name in ["ipc_app/meari_ipc/main/ppsapp.c", "ipc_app/tuya_ipc/tuya_app/pps_tuya_app.c"]:
        phase = None
        conditions = []
        for number, line in enumerate((root / name).read_text().splitlines(), 1):
            if re.match(r"ppsdev_init_plugins_t .*\[\] =", line):
                phase = re.search(r"plugins_(core|user|after)", line).group(1)
                conditions = []
            if phase is None:
                continue
            if line.startswith("#if"):
                conditions.append(line.strip())
            elif line.startswith("#else") and conditions:
                conditions[-1] = "else of " + conditions[-1]
            elif line.startswith("#endif") and conditions:
                conditions.pop()
            match = re.search(r'\{"([^"]+)",\s*(?:\(void \*\))?\s*(\w+),\s*(\d+),\s*(\d+),\s*(\d+)\}', line)
            if match and not line.lstrip().startswith("//"):
                plugins.append([name, number, phase, *match.groups(), " && ".join(conditions)])
            if line.strip() == "};":
                phase = None
    with (out / "startup-plugins.tsv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(["source", "line", "phase", "name", "handler", "by_thread", "stack_kib", "priority", "preprocessor_context"])
        writer.writerows(plugins)
    summary = {
        "source_root": str(root),
        "line_count_definition": "LF-delimited physical lines, counting final unterminated line; includes comments and blanks",
        "total_files": len(rows), "extensions": dict(sorted(extensions.items())),
        "source_files": sum(r[4] for r in rows),
        "source_lines": sum(r[2] for r in rows if r[4]),
        "tuya_sdk_source_files": sdk_files,
        "tuya_sdk_source_lines": sdk_lines,
        "startup_plugin_rows": len(plugins),
        "modules": dict(sorted(modules.items())),
        "largest_source_files": [{"path": r[0], "lines": r[2]} for r in sorted(
            (r for r in rows if r[4]), key=lambda r: (-r[2], r[0]))[:12]],
        "meari_cmake_profiles": len(list((root / "ipc_app/meari_ipc/scripts/buildconf").glob("*.cmake"))),
        "tuya_cmake_profiles": len(list((root / "ipc_app/tuya_ipc/cmake_config").glob("*.cmake"))),
        "capa_unique_v_tokens": len(set(re.findall(r"\bV_[A-Z0-9_]+\b", capa))),
        "missing_literal_cmake_references": len(missing),
        "missing_path_method": "PROJECT_TOP_DIR literal paths in uncommented text only; no CMake condition or variable evaluation",
        "shell_syntax_checks": shell_checks,
        "source_has_git_directory": (root / ".git").exists(),
        "dependency_presence": {name: (root / name).exists() for name in [
            "arch", "ipc_app/meari_ipc/sdk", "pps_hal/x86_64", "include/storage",
            "include/nkit", "components/pps_nkit_module/pps_nkit_wlan.c",
            "ipc_app/meari_ipc/meari_platform.conf", "ipc_app/meari_ipc/meari_software_platform.conf",
            "common/net/mongoose.c"]},
    }
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({key: summary[key] for key in ["total_files", "source_files", "source_lines",
          "missing_literal_cmake_references", "capa_unique_v_tokens"]}, ensure_ascii=False))
    print("bash -n:", sum(c["exit_code"] == 0 for c in shell_checks), "/", len(shell_checks))


if __name__ == "__main__":
    main()
