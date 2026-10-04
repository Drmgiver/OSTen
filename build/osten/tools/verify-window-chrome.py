#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check OSTen's visible window chrome without assuming a fixed window offset."""
from pathlib import Path
import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
# Finder windows can be tiled at different vertical offsets. All visible
# title tabs sit below the menu bar and above the first row of folder icons.
chrome = image.crop((0, 25, min(600, image.width), min(85, image.height)))
yellow_pixels = sum(
    red >= 220 and 160 <= green <= 230 and blue <= 130
    for red, green, blue in chrome.getdata()
)
platinum_pixels = sum(
    max(pixel) - min(pixel) <= 8 and 160 <= min(pixel) <= 245
    for pixel in chrome.getdata()
)
print(f'Finder window-chrome yellow pixels: {yellow_pixels}')
print(f'Finder Platinum chrome pixels: {platinum_pixels}')
if yellow_pixels > 300:
    raise SystemExit('Finder is still using the yellow-tab decorator')
if platinum_pixels < 1800:
    raise SystemExit('Finder Platinum title bar did not appear')
