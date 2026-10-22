#!/usr/bin/env python3
"""Generate a tiny synthetic dataset to exercise the EdgeVision offline evaluator.

No third-party dependencies (pure stdlib). Produces PPM photos (the framework's
native, dependency-free image format) plus annotations.json. The colours map to
the x86 reference detector's two classes: red -> class 0 (person), blue ->
class 1 (helmet).

Run:
    python3 gen_fixtures.py            # writes ./fixtures/{images/*.ppm, annotations.json}
"""
import json
import os
import struct

W, H = 320, 240
GRAY = (128, 128, 128)
RED = (230, 40, 40)    # class 0
BLUE = (40, 40, 230)   # class 1


def new_canvas():
    buf = bytearray()
    for _ in range(W * H):
        buf += bytes(GRAY)
    return buf


def draw_rect(buf, color, cx, cy, bw, bh):
    x0 = int((cx - bw / 2) * W)
    y0 = int((cy - bh / 2) * H)
    x1 = int((cx + bw / 2) * W)
    y1 = int((cy + bh / 2) * H)
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(W - 1, x1), min(H - 1, y1)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            off = (y * W + x) * 3
            buf[off:off + 3] = bytes(color)


def write_ppm(path, buf):
    with open(path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (W, H))
        f.write(bytes(buf))


def make_images(out_dir):
    images = []
    img_dir = os.path.join(out_dir, "images")
    os.makedirs(img_dir, exist_ok=True)

    def scene(name, rects, boxes):
        buf = new_canvas()
        for (color, cx, cy, bw, bh) in rects:
            draw_rect(buf, color, cx, cy, bw, bh)
        write_ppm(os.path.join(img_dir, name), buf)
        images.append({"file": name, "boxes": boxes})

    # 1) one red (person) rectangle
    scene("img1_person.ppm",
          [(RED, 0.30, 0.40, 0.20, 0.30)],
          [{"class": 0, "x": 0.30, "y": 0.40, "w": 0.20, "h": 0.30}])

    # 2) one blue (helmet) rectangle
    scene("img2_helmet.ppm",
          [(BLUE, 0.62, 0.50, 0.16, 0.22)],
          [{"class": 1, "x": 0.62, "y": 0.50, "w": 0.16, "h": 0.22}])

    # 3) empty / negative scene
    scene("img3_empty.ppm", [], [])

    # 4) red rectangle + a small red speck (below the min-area noise floor)
    scene("img4_person_noise.ppm",
          [(RED, 0.35, 0.45, 0.20, 0.30), (RED, 0.85, 0.15, 0.03, 0.03)],
          [{"class": 0, "x": 0.35, "y": 0.45, "w": 0.20, "h": 0.30}])

    # 5) two red rectangles (two persons)
    scene("img5_two_persons.ppm",
          [(RED, 0.25, 0.40, 0.15, 0.25), (RED, 0.70, 0.55, 0.15, 0.25)],
          [{"class": 0, "x": 0.25, "y": 0.40, "w": 0.15, "h": 0.25},
           {"class": 0, "x": 0.70, "y": 0.55, "w": 0.15, "h": 0.25}])

    # 6) one blue + one red (helmet + person)
    scene("img6_both.ppm",
          [(BLUE, 0.55, 0.45, 0.16, 0.22), (RED, 0.30, 0.55, 0.18, 0.28)],
          [{"class": 1, "x": 0.55, "y": 0.45, "w": 0.16, "h": 0.22},
           {"class": 0, "x": 0.30, "y": 0.55, "w": 0.18, "h": 0.28}])

    # 7) a red blob the human did NOT annotate -> false positive
    scene("img7_fp.ppm",
          [(RED, 0.45, 0.50, 0.20, 0.25)],
          [])

    # 8) ground truth asks for a person, but it is a tiny blob below the
    #    detector's min-area floor -> false negative (recall < 1)
    scene("img8_fn.ppm",
          [(RED, 0.50, 0.50, 0.03, 0.03)],
          [{"class": 0, "x": 0.50, "y": 0.50, "w": 0.03, "h": 0.03}])

    anno = {"class_names": ["person", "helmet"], "images": images}
    with open(os.path.join(out_dir, "annotations.json"), "w") as f:
        json.dump(anno, f, indent=2)
    print("wrote %d images + annotations.json into %s" % (len(images), out_dir))


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    make_images(os.path.join(here, "fixtures"))
