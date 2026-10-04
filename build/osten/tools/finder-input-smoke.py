#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise Finder through QEMU input and capture its native test results."""
import json
from pathlib import Path
import socket
import sys
import time
from PIL import Image

monitor, output = sys.argv[1:]
output = Path(output)
width, height = Image.open(output / 'finder.ppm').size
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


def key(name):
    command('human-monitor-command', {'command-line': f'sendkey {name}'})
    time.sleep(2)


def screenshot(name):
    command('screendump', {'filename': str(output / f'{name}.ppm')})


# Root folder context menu, then exercise shortcuts in the focused
# Applications Finder window.
click(350, 320, 'right')
screenshot('context-menu')
key('esc')
click(215, 95)
key('ret')
screenshot('finder')

key('alt-n')
screenshot('new-folder-shortcut')
image = Image.open(output / 'new-folder-shortcut.ppm').convert('RGB')
folder_area = image.crop((84, 85, 562, 438))
yellow_pixels = sum(
    r >= 220 and 160 <= g <= 230 and b <= 130
    for r, g, b in folder_area.getdata()
)
if yellow_pixels < 300:
    raise RuntimeError('Command-N did not create a folder in the focused window')

key('alt-a')
screenshot('select-all-shortcut')
image = Image.open(output / 'select-all-shortcut.ppm').convert('RGB')
folder_area = image.crop((84, 85, 562, 438))
selected_pixels = sum(
    r <= 10 and g <= 10 and 100 <= b <= 150
    for r, g, b in folder_area.getdata()
)
if selected_pixels < 300:
    raise RuntimeError('Command-A did not select the focused window contents')

key('alt-w')
time.sleep(2)
screenshot('close-shortcut')
closed = Image.open(output / 'close-shortcut.ppm').convert('RGB')
root = Image.open(output / 'finder.ppm').convert('RGB')
white_pixels = lambda image: sum(
    r >= 235 and g >= 235 and b >= 235 for r, g, b in image.getdata()
)
if white_pixels(closed) > white_pixels(root) + 10000:
    raise RuntimeError('Command-W did not close the focused Finder window')

# Reopen Applications, then launch the native transfer test app with Command-O.
click(215, 95)
key('ret')
click(150, 128)
# A focused Finder folder must route the global Command-O shortcut to its
# selected item, not back to the desktop or the previously active window.
key('alt-o')
for attempt in range(30):
    log = (output / 'serial.log').read_text(errors='replace')
    if 'OSTEN_TRANSFER_TESTS_PASS 16' in log or 'OSTEN_TRANSFER_TESTS_FAIL' in log:
        break
    time.sleep(1)
screenshot('transfer-tests')
if 'OSTEN_TRANSFER_TESTS_PASS 16' not in log:
    raise RuntimeError('Native transfer tests did not pass:\\n' + log[-8000:])
connection.close()
