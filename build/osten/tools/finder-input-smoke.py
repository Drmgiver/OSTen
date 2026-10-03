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


# Root folder context menu, then Applications and its test executable.
click(350, 320, 'right')
screenshot('context-menu')
key('esc')
click(215, 95)
key('alt-o')
click(135, 125)
key('alt-o')
for attempt in range(30):
    log = (output / 'serial.log').read_text(errors='replace')
    if 'OSTEN_TRANSFER_TESTS_PASS 16' in log or 'OSTEN_TRANSFER_TESTS_FAIL' in log:
        break
    time.sleep(1)
screenshot('transfer-tests')
connection.close()
