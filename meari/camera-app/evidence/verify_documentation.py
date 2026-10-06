#!/usr/bin/env python3
"""Validate local Markdown references and compare a regenerated source snapshot."""
import argparse
import csv
import hashlib
import json
import re
from pathlib import Path
from urllib.parse import unquote


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recheck", type=Path, required=True,
                        help="Output of a fresh audit_snapshot.py run")
    args = parser.parse_args()
    evidence = Path(__file__).resolve().parent
    root = evidence.parent
    output = evidence / "documentation-checks.json"
    errors = []
    documents = []
    source_links = 0
    total_links = 0
    snapshot_checks = {}
    for name in ("source-inventory.tsv", "startup-plugins.tsv",
                 "missing-cmake-paths.tsv", "summary.json"):
        fresh = args.recheck / name
        same = fresh.is_file() and (evidence / name).read_bytes() == fresh.read_bytes()
        snapshot_checks[name] = same
        if not same:
            errors.append(f"Snapshot differs or is absent: {name}")
    summary = json.loads((evidence / "summary.json").read_text())
    source_root = Path(summary["source_root"])
    with (evidence / "source-inventory.tsv").open(newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    stats = {
        "file_count_matches": len(rows) == summary["total_files"],
        "source_count_matches": sum(int(r["c_h_cpp"]) for r in rows) == summary["source_files"],
        "source_lines_match": sum(int(r["physical_lines"]) for r in rows
                                  if int(r["c_h_cpp"])) == summary["source_lines"],
    }
    for name, field in (("startup-plugins.tsv", "startup_plugin_rows"),
                        ("missing-cmake-paths.tsv", "missing_literal_cmake_references")):
        with (evidence / name).open(newline="") as handle:
            count = len(list(csv.DictReader(handle, delimiter="\t")))
        stats[name + "_count_matches"] = count == summary[field]
    if not all(stats.values()):
        errors.append("Evidence row counts disagree with summary")
    for path in (root / "camera-app-analysis.md", root / "核验记录.md"):
        body = path.read_text()
        active = []
        fence = None
        for number, line in enumerate(body.splitlines(), 1):
            match = re.match(r"^\s*(`{3,}|~{3,})", line)
            if match:
                marker = match.group(1)
                if fence is None:
                    fence = marker
                elif marker[0] == fence[0] and len(marker) >= len(fence):
                    fence = None
                continue
            if fence is None:
                active.append((number, line))
        if fence is not None:
            errors.append(f"{path.name}: unclosed code fence")
        anchors = set()
        slug_counts = {}
        for _, line in active:
            heading = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
            if heading:
                slug = re.sub(r"[^\w\s-]", "", heading.group(1).lower()).replace(" ", "-")
                duplicate = slug_counts.get(slug, 0)
                slug_counts[slug] = duplicate + 1
                anchors.add(slug if duplicate == 0 else f"{slug}-{duplicate}")
        checked = 0
        for number, line in active:
            for match in re.finditer(r"\[[^\]]+\]\(([^)]+)\)", line):
                target = unquote(match.group(1).strip().strip("<>"))
                checked += 1
                total_links += 1
                if target.startswith("#"):
                    if target[1:] not in anchors:
                        errors.append(f"{path.name}:{number}: unknown anchor {target}")
                    continue
                if re.match(r"^[a-zA-Z]+://", target):
                    errors.append(f"{path.name}:{number}: unverified external link {target}")
                    continue
                location, separator, reference = target.rpartition(":")
                line_number = int(reference) if separator and reference.isdigit() else None
                filename = location if line_number is not None else target
                linked = Path(filename)
                if not linked.is_absolute():
                    linked = path.parent / linked
                linked = linked.resolve()
                # The output is created after validation; its self-reference is allowed.
                if linked == output:
                    continue
                if not linked.is_file():
                    errors.append(f"{path.name}:{number}: missing file {target}")
                    continue
                if source_root == linked or source_root in linked.parents:
                    source_links += 1
                if line_number is not None:
                    data = linked.read_bytes()
                    count = data.count(b"\n") + int(bool(data) and not data.endswith(b"\n"))
                    if not 1 <= line_number <= count:
                        errors.append(f"{path.name}:{number}: out-of-range source line {target}")
        documents.append({"path": path.name, "lines": len(body.splitlines()),
                          "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                          "checked_links": checked, "code_fences_balanced": fence is None})
    result = {
        "documents": documents,
        "checked_links": total_links,
        "source_links": source_links,
        "snapshot_comparison_directory": str(args.recheck.resolve()),
        "snapshot_files_identical": snapshot_checks,
        "source_snapshot_identical": all(snapshot_checks.values()),
        "statistics_checks": stats,
        "errors": errors,
        "passed": not errors,
        "scope": "Local link existence, source line bounds, heading anchors, fences, document hashes and reproducible evidence; not semantic proof or firmware tests",
    }
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
