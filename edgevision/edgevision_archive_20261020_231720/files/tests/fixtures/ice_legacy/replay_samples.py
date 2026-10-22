#!/usr/bin/env python3
"""Replay image files with explicit original-pixel ROI recipes. Requires Pillow."""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--rois", type=Path, default=Path(__file__).with_name("sample_rois.json"))
    parser.add_argument(
        "--mode", choices=("fixed_surface", "manual_boxes"), default="fixed_surface"
    )
    parser.add_argument("--saturation-max", type=int, default=80)
    parser.add_argument("--value-min", type=int, default=180)
    args = parser.parse_args()
    if not 0 <= args.saturation_max <= 255 or not 0 <= args.value_min <= 255:
        parser.error("HSV thresholds must be within 0..255")
    args.out.mkdir(parents=True, exist_ok=True)
    recipes = json.loads(args.rois.read_text(encoding="utf-8"))
    results = []
    previews = []
    binary = args.binary.resolve()
    for index, recipe in enumerate(recipes["images"]):
        image = Image.open(args.samples / recipe["file"]).convert("RGB")
        expected = tuple(recipe["reference_size"])
        if image.size != expected:
            raise ValueError(f"{recipe['file']}: expected {expected}, got {image.size}")
        width, height = image.size
        normalized = [
            value / (width - 1 if axis == 0 else height - 1)
            for point in recipe["polygon_pixels"]
            for axis, value in enumerate(point)
        ]
        if len(normalized) < 6 or any(not 0 <= value <= 1 for value in normalized):
            raise ValueError(f"{recipe['file']}: invalid ROI")
        points = recipe["polygon_pixels"]
        left = min(p[0] for p in points)
        top = min(p[1] for p in points)
        box_width = max(p[0] for p in points) - left + 1
        box_height = max(p[1] for p in points) - top + 1
        if args.mode == "manual_boxes":
            normalized = [
                value
                for x, y in points
                for value in ((x - left) / (box_width - 1), (y - top) / (box_height - 1))
            ]
        prefix = args.out / f"{index:02d}"
        ppm = prefix.with_suffix(".ppm")
        image.save(ppm)
        config = prefix.with_suffix(".yaml")
        geometry = (
            f"ice_surface_polygon: {json.dumps(normalized)}\n"
            if args.mode == "fixed_surface"
            else "ice_objects:\n  - id: 0\n    name: winch\n"
            f"    surface_polygon: {json.dumps(normalized)}\n"
        )
        config.write_text(
            "# Illustrative ROI; not independent ground truth or field calibration.\n"
            "task: winch_ice\nmodule_id: 0x0008\ncamera:\n  video_channel: [0]\n"
            "inference_enabled: false\nground:\n  enabled: false\n"
            f"ice_mode: {args.mode}\n"
            f"ice_reference_width: {width}\nice_reference_height: {height}\n"
            f"ice_saturation_max: {args.saturation_max}\nice_value_min: {args.value_min}\narea_ratio: 0.5\n"
            + geometry,
            encoding="utf-8",
        )
        command = [str(binary), str(config), str(ppm), str(prefix)]
        if args.mode == "manual_boxes":
            sequence = prefix.with_suffix(".json")
            sequence.write_text(
                json.dumps(
                    {
                        "frames": [
                            {
                                "angle": 0,
                                "working": 1,
                                "boxes": [
                                    {
                                        "id": 0,
                                        "x": left,
                                        "y": top,
                                        "width": box_width,
                                        "height": box_height,
                                    }
                                ],
                            }
                        ]
                    }
                )
            )
            command += ["--sequence", str(sequence)]
        run = subprocess.run(command, capture_output=True, text=True)
        prefix.with_name(prefix.name + "_replay.log").write_text(
            run.stdout + run.stderr, encoding="utf-8"
        )
        if run.returncode not in (0, 1):
            raise RuntimeError(run.stdout + run.stderr)
        result = json.loads(prefix.with_name(prefix.name + "_result.json").read_text())
        obj = result["objects"][0]
        result.update(roi_pixels=obj["surface_pixels"], snow_pixels=obj["snow_pixels"])
        result.update(file=recipe["file"], scene=recipe["scene"], roi_note=recipe.get("note", ""))
        results.append(result)
        for path in list(args.out.glob(prefix.name + "_*.p?m")):
            with Image.open(path) as output:
                output.save(path.with_suffix(".png"))
        overlay = Image.open(prefix.with_name(prefix.name + "_overlay.png")).convert("RGB")
        overlay.thumbnail((500, 360))
        tile = Image.new("RGB", (520, 420), "white")
        tile.paste(overlay, (10, 45))
        draw = ImageDraw.Draw(tile)
        draw.text(
            (10, 5),
            f"{index:02d} scene={recipe['scene']} ratio={result['ratio']:.4f} alarm={result['alarm']}",
            fill="black",
        )
        draw.text(
            (10, 25),
            f"ROI={result['roi_pixels']} candidate={result['snow_pixels']} status={result['status']}",
            fill="black",
        )
        previews.append(tile)
    contact = Image.new("RGB", (1040, 420 * ((len(previews) + 1) // 2)), "#eeeeee")
    for index, tile in enumerate(previews):
        contact.paste(tile, ((index % 2) * 520, (index // 2) * 420))
    contact.save(args.out / "contact.png")
    summary = {
        "description": recipes["description"],
        "saturation_max": args.saturation_max,
        "value_min": args.value_min,
        "images": len(results),
        "independent_scenes": len({r["scene"] for r in results}),
        "results": results,
    }
    (args.out / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
