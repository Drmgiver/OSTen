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
def is_platinum(pixel):
    return max(pixel) - min(pixel) <= 8 and 160 <= min(pixel) <= 245


platinum_pixels = sum(is_platinum(pixel) for pixel in chrome.getdata())
# A BeOS-style tab is much narrower than the original full-width Platinum
# bar. Confirm that the gray pixels form an actual horizontal title-tab
# surface, rather than merely accepting a few incidental gray pixels.
longest_gray_run = 0
for y in range(chrome.height):
    row_run = 0
    for x in range(chrome.width):
        if is_platinum(chrome.getpixel((x, y))):
            row_run += 1
            longest_gray_run = max(longest_gray_run, row_run)
        else:
            row_run = 0
print(f'Finder window-chrome yellow pixels: {yellow_pixels}')
print(f'Finder Platinum chrome pixels: {platinum_pixels}')
print(f'Finder tab longest gray run: {longest_gray_run}')
if yellow_pixels > 300:
    raise SystemExit('Finder is still using the yellow-tab decorator')
# Previously required 1,800 gray pixels, which incorrectly rejected the
# compact OSTen tab (1,501 pixels in the observed boot evidence). Both
# tests together require a visible Platinum-colored title-tab region.
if platinum_pixels < 650 or longest_gray_run < 65:
    raise SystemExit('Finder Platinum window tab did not appear')
