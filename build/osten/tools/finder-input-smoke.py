#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise Finder through QEMU input and capture its native test results."""
import csv
import io
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
from PIL import Image

monitor, output = sys.argv[1:3]
mode = sys.argv[3] if len(sys.argv) > 3 else 'test'
output = Path(output)
width, height = Image.open(output / 'screenshot.ppm').size
connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
connection.settimeout(15)
connection.connect(monitor)
stream = connection.makefile('rwb', buffering=0)
json.loads(stream.readline())  # QMP greeting


def command(name, arguments=None):
    request = {'execute': name}
    if arguments is not None:
        request['arguments'] = arguments
    stream.write(json.dumps(request).encode() + b'\n')
    while True:
        response = json.loads(stream.readline())
        if 'error' in response:
            raise RuntimeError(response['error'])
        if 'return' in response:
            return response['return']


command('qmp_capabilities')


def click(x, y, button='left'):
    command('input-send-event', {'events': [
        {'type': 'abs', 'data': {'axis': 'x', 'value': round(x * 32767 / (width - 1))}},
        {'type': 'abs', 'data': {'axis': 'y', 'value': round(y * 32767 / (height - 1))}},
    ]})
    time.sleep(0.3)
    command('input-send-event', {'events': [
        {'type': 'btn', 'data': {'down': True, 'button': button}},
    ]})
    time.sleep(0.2)
    command('input-send-event', {'events': [
        {'type': 'btn', 'data': {'down': False, 'button': button}},
    ]})
    time.sleep(1)


def double_click(x, y):
    command('input-send-event', {'events': [
        {'type': 'abs', 'data': {'axis': 'x', 'value': round(x * 32767 / (width - 1))}},
        {'type': 'abs', 'data': {'axis': 'y', 'value': round(y * 32767 / (height - 1))}},
    ]})
    for _ in range(2):
        command('input-send-event', {'events': [
            {'type': 'btn', 'data': {'down': True, 'button': 'left'}},
        ]})
        time.sleep(0.1)
        command('input-send-event', {'events': [
            {'type': 'btn', 'data': {'down': False, 'button': 'left'}},
        ]})
        time.sleep(0.15)
    time.sleep(2)


def key(name):
    command('human-monitor-command', {'command-line': f'sendkey {name}'})
    time.sleep(2)


def screenshot(name):
    command('screendump', {'filename': str(output / f'{name}.ppm')})


def locate_text(image_path, target, min_top=60):
    image = Image.open(image_path).convert('RGB')
    scaled = image.resize((image.width * 3, image.height * 3),
        Image.Resampling.NEAREST)
    data = io.BytesIO()
    scaled.save(data, format='PNG')
    result = subprocess.run(
        ['tesseract', 'stdin', 'stdout', '--psm', '11', 'tsv'],
        input=data.getvalue(), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=True)
    rows = csv.DictReader(io.StringIO(result.stdout.decode()), delimiter='\t')
    target = ''.join(character for character in target.lower()
        if character.isalnum())
    matches = []
    for row in rows:
        text = ''.join(character for character in row['text'].lower()
            if character.isalnum())
        if target not in text:
            continue
        top = int(row['top']) / 3
        if top < min_top:
            continue
        confidence = float(row['conf'])
        x = (int(row['left']) + int(row['width']) / 2) / 3
        y = (int(row['top']) + int(row['height']) / 2) / 3
        matches.append((top, confidence, x, y))
    if not matches:
        raise RuntimeError(f'Could not find "{target}" in {image_path}')
    # A file or folder label is lower in the window than menu and title text.
    _, _, x, y = max(matches, key=lambda item: (item[0], item[1]))
    return round(x), round(y)


def open_folder_named(name, source_image):
    x, y = locate_text(source_image, name)
    double_click(x, y)


# Open the desktop volume directly. Command-O depends on the floating menu bar
# having keyboard focus, which the boot sequence does not guarantee.
double_click(width - 56, 75)
screenshot('finder')
if mode == '--open-only':
    connection.close()
    raise SystemExit(0)
if mode != 'test':
    raise RuntimeError(f'Unknown smoke mode: {mode}')


# Exercise the root folder context menu, then shortcuts in Applications.
click(350, 320, 'right')
screenshot('context-menu')
key('esc')
open_folder_named('Applications', output / 'finder.ppm')
screenshot('applications')

key('alt-n')
screenshot('new-folder-shortcut')
image = Image.open(output / 'new-folder-shortcut.ppm').convert('RGB')
folder_area = image.crop((56, 57, min(562, width), min(438, height)))
yellow_pixels = sum(
    red >= 220 and 160 <= green <= 230 and blue <= 130
    for red, green, blue in folder_area.getdata()
)
if yellow_pixels < 300:
    raise RuntimeError('Command-N did not create a folder in the focused window')

key('alt-a')
screenshot('select-all-shortcut')
image = Image.open(output / 'select-all-shortcut.ppm').convert('RGB')
folder_area = image.crop((56, 57, min(562, width), min(438, height)))
selected_pixels = sum(
    red <= 10 and green <= 10 and 100 <= blue <= 150
    for red, green, blue in folder_area.getdata()
)
if selected_pixels < 300:
    raise RuntimeError('Command-A did not select the focused window contents')

key('alt-w')
time.sleep(2)
screenshot('close-shortcut')
closed = Image.open(output / 'close-shortcut.ppm').convert('RGB')
opened = Image.open(output / 'applications.ppm').convert('RGB')
white_pixels = lambda image: sum(
    red >= 235 and green >= 235 and blue >= 235
    for red, green, blue in image.getdata()
)
if white_pixels(closed) > white_pixels(opened) + 10000:
    raise RuntimeError('Command-W did not close the focused Finder window')

screenshot('root-after-close')
# Applications remains selected in the parent window after Command-W.
# Double-click its stable icon cell; reverse-video text is unreliable for OCR.
double_click(215, 135)
screenshot('applications-reopened')
# Select the native test app by its visible label, then open it with Command-O.
x, y = locate_text(output / 'applications-reopened.ppm', 'Finder')
click(x, y)
key('alt-o')
for attempt in range(30):
    log = (output / 'serial.log').read_text(errors='replace')
    if 'OSTEN_TRANSFER_TESTS_PASS 16' in log or 'OSTEN_TRANSFER_TESTS_FAIL' in log:
        break
    time.sleep(1)
screenshot('transfer-tests')
if 'OSTEN_TRANSFER_TESTS_PASS 16' not in log:
    raise RuntimeError('Native transfer tests did not pass:\n' + log[-8000:])
connection.close()
