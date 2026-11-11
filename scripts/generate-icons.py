#!/usr/bin/env python3
"""
Generate multi-platform icon assets (Linux hicolor PNG/SVG, Windows ICO, macOS ICNS)
from the source SVG logo: ui/resources/icons/brand/app_logo.svg.
"""

import os
import shutil
import struct
import subprocess
import sys

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    svg_path = os.path.join(root_dir, "ui", "resources", "icons", "brand", "app_logo.svg")
    if not os.path.exists(svg_path):
        print(f"Error: {svg_path} not found", file=sys.stderr)
        sys.exit(1)

    sizes = [16, 24, 32, 48, 64, 128, 256, 512]
    png_data = {}

    for s in sizes + [1024]:
        cmd = ["rsvg-convert", "-w", str(s), "-h", str(s), svg_path]
        png_bytes = subprocess.check_output(cmd)
        png_data[s] = png_bytes
        if s in sizes:
            d = os.path.join(root_dir, "packaging", "linux", "icons", "hicolor", f"{s}x{s}", "apps")
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "io.hephaestus.svm.png"), "wb") as f:
                f.write(png_bytes)

    # Linux Scalable SVG
    scalable_dir = os.path.join(root_dir, "packaging", "linux", "icons", "hicolor", "scalable", "apps")
    os.makedirs(scalable_dir, exist_ok=True)
    shutil.copyfile(svg_path, os.path.join(scalable_dir, "io.hephaestus.svm.svg"))

    # Windows svm.ico
    win_dir = os.path.join(root_dir, "packaging", "windows")
    os.makedirs(win_dir, exist_ok=True)
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_images = [png_data[s] for s in ico_sizes]

    header = struct.pack("<HHH", 0, 1, len(ico_images))
    offset = 6 + 16 * len(ico_images)
    entries = []
    for s, img in zip(ico_sizes, ico_images):
        w = 0 if s == 256 else s
        h = 0 if s == 256 else s
        entries.append(struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(img), offset))
        offset += len(img)

    with open(os.path.join(win_dir, "svm.ico"), "wb") as f:
        f.write(header)
        for e in entries:
            f.write(e)
        for img in ico_images:
            f.write(img)

    # macOS svm.icns
    mac_dir = os.path.join(root_dir, "packaging", "macos")
    os.makedirs(mac_dir, exist_ok=True)
    icns_elements = [
        (b"ic04", png_data[16]),
        (b"ic05", png_data[32]),
        (b"ic07", png_data[128]),
        (b"ic08", png_data[256]),
        (b"ic09", png_data[512]),
        (b"ic10", png_data[1024]),
    ]

    body = bytearray()
    for tag, data in icns_elements:
        chunk_len = 8 + len(data)
        body.extend(tag)
        body.extend(struct.pack(">I", chunk_len))
        body.extend(data)

    total_len = 8 + len(body)
    icns_header = b"icns" + struct.pack(">I", total_len)

    with open(os.path.join(mac_dir, "svm.icns"), "wb") as f:
        f.write(icns_header + body)

    print("Successfully generated all icons for Linux, Windows, and macOS.")

if __name__ == "__main__":
    main()
