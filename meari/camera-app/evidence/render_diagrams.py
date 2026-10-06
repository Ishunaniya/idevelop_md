#!/usr/bin/env python3
"""Draw portable PNG report diagrams with Pillow and a local Chinese font."""
import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", default="/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
    args = parser.parse_args()
    assets = Path(__file__).resolve().parent.parent / "assets"
    assets.mkdir(exist_ok=True)
    fonts = {size: ImageFont.truetype(args.font, size) for size in (20, 23, 27, 34)}
    ink, blue = "#24364b", "#3975a7"

    def canvas(size, title):
        im = Image.new("RGB", size, "white")
        d = ImageDraw.Draw(im)
        d.text((40, 26), title, font=fonts[34], fill=ink)
        return im, d

    def label(d, point, content, size=23):
        d.multiline_text(point, content, font=fonts[size], fill=ink,
                         anchor="mm", align="center", spacing=8)

    def box(d, rect, content, background=False):
        d.rounded_rectangle(rect, radius=12, fill="#eef6fc" if not background else "#f3f6f9",
                            outline=blue, width=2)
        label(d, ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2), content)

    def arrow(d, points, dashed=False):
        for start, end in zip(points, points[1:]):
            if dashed:
                length = math.dist(start, end)
                if not length:
                    continue
                for offset in range(0, math.ceil(length), 16):
                    a, b = offset / length, min(offset + 9, length) / length
                    d.line([(start[0] + (end[0]-start[0])*t,
                             start[1] + (end[1]-start[1])*t) for t in (a, b)], fill=blue, width=3)
            else:
                d.line([start, end], fill=blue, width=3)
        start, end = points[-2:]
        angle = math.atan2(end[1]-start[1], end[0]-start[0])
        corners = [(end[0]-14*math.cos(angle+shift), end[1]-14*math.sin(angle+shift))
                   for shift in (-0.45, 0.45)]
        d.polygon([end, *corners], fill=blue)

    im, d = canvas((1260, 950), "启动框架：主线程提交顺序与后台任务")
    main_boxes = [(70, y, 500, y+75) for y in (110, 265, 420, 575, 730)]
    for rect, content in zip(main_boxes, ("main()", "遍历 core 表", "遍历 user 表",
                                          "遍历 after 表", "主线程 sleep(10000) 循环")):
        box(d, rect, content)
    for first, second in zip(main_boxes, main_boxes[1:]):
        arrow(d, [(285, first[3]), (285, second[1])])
    for y, content in ((265, "看门狗、PTZ、SD 等\n后台任务"),
                       (420, "网络、RTSP、ONVIF 等\n后台任务"),
                       (575, "云 SDK 与后续任务")):
        box(d, (750, y, 1190, y+75), content, True)
        arrow(d, [(500, y+37), (750, y+37)], True)
        label(d, (625, y+5), "创建线程" if y != 575 else "创建线程 / 直调", 20)
    for y, content in ((340, "模块内部事件 / 状态"), (495, "联网等待等内部条件")):
        arrow(d, [(970, y), (970, y+80)], True)
        label(d, (1090, y+40), content, 20)
    label(d, (630, 860), "实线：主线程遍历顺序；虚线：后台启动及模块内部协调。", 23)
    label(d, (630, 902), "框架无统一 join / barrier；表遍历结束不代表后台服务全部就绪。", 23)
    im.save(assets / "startup-framework.png")

    im, d = canvas((1500, 850), "媒体分发：适配、回调与消费方")
    for rect, content in (((420, 105, 900, 180), "传感器与外部 SoC 媒体库"),
                          ((420, 260, 900, 335), "pps_device_media 适配"),
                          ((420, 415, 900, 490), "主流、子流、音频回调"),
                          ((1110, 260, 1460, 335), "快照与检测回调"),
                          ((1110, 415, 1460, 490), "事件、告警、云推送")):
        box(d, rect, content)
    arrow(d, [(660, 180), (660, 260)])
    arrow(d, [(660, 335), (660, 415)])
    arrow(d, [(900, 297), (1110, 297)])
    arrow(d, [(1285, 335), (1285, 415)])
    destinations = [(40, "RTSP circular buffer\n与 RTP"), (410, "Meari SDK 回调"),
                    (780, "Tuya SDK\nring buffer"), (1150, "SD 录像写入")]
    arrow(d, [(660, 490), (660, 550)])
    d.line([(200, 550), (1310, 550)], fill=blue, width=3)
    for x, content in destinations:
        arrow(d, [(x+160, 550), (x+160, 620)])
        box(d, (x, 620, x+320, 720), content, True)
    label(d, (750, 770), "分支依最终宏、回调注册和运行状态启用；SDK 内部传输需对应实现核验。", 23)
    label(d, (750, 812), "同节文字版保留视频、音频、快照、检测与本地录像的详细关系。", 23)
    im.save(assets / "media-distribution.png")


if __name__ == "__main__":
    main()
