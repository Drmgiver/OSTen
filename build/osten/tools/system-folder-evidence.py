#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture the actual Finder and System icons from a saved OSTen QEMU image.

This test reboots an existing image without rebuilding OSTen. It opens the
startup volume, then System Folder, and verifies both file icons visually.
"""
import csv
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

from PIL import Image


def locate_label(path, target, min_top=60):
    image = Image.open(path).convert('RGB')
    large = image.resize((image.width * 3, image.height * 3),
        Image.Resampling.NEAREST)
    data = io.BytesIO()
    large.save(data, format='PNG')
    result = subprocess.run(
        ['tesseract', 'stdin', 'stdout', '--psm', '11', 'tsv'],
        input=data.getvalue(), stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=True)
    matches = []
    for row in csv.DictReader(io.StringIO(result.stdout.decode()),
            delimiter='\t'):
        word = ''.join(ch for ch in row['text'].lower() if ch.isalnum())
        if word != target.lower():
            continue
        top = int(row['top']) / 3
        if top < min_top:
            continue
        x = (int(row['left']) + int(row['width']) / 2) / 3
        y = (int(row['top']) + int(row['height']) / 2) / 3
        matches.append((top, float(row['conf']), round(x), round(y)))
    if not matches:
        raise RuntimeError(f'Cannot find {target!r} in {path}')
    _, _, x, y = max(matches, key=lambda value: value[:2])
    return x, y


def verify_artwork(path, name, label_position=None):
    x, y = (label_position if label_position is not None
        else locate_label(path, name, min_top=80))
    screen = Image.open(path).convert('RGB')
    # Finder places each 40px icon immediately above its label.
    icon = screen.crop((x - 25, y - 59, x + 25, y - 15))
    blue = sum(red < 190 and green > 75 and blue > red + 35
        and blue > green + 10 for red, green, blue in icon.getdata())
    print(f'{name}: label=({x}, {y}); blue icon pixels={blue}', flush=True)
    if blue < 50:
        raise RuntimeError(f'{name} lacks the approved blue icon artwork')


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: system-folder-evidence.py IMAGE OUTPUT_DIR')
    image = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not image.is_file():
        raise RuntimeError(f'Missing saved OSTen image: {image}')

    qmp = output / 'system-folder-qmp.sock'
    qmp.unlink(missing_ok=True)
    serial = output / 'system-folder-serial.log'
    log = (output / 'system-folder-qemu.log').open('w')
    qemu = subprocess.Popen([
        'qemu-system-x86_64', '-m', '2048', '-smp', '2', '-snapshot',
        '-drive', f'file={image},format=raw,if=ide',
        '-vga', 'std', '-display', 'none', '-usb', '-device', 'usb-tablet',
        '-qmp', f'unix:{qmp},server=on,wait=off',
        '-serial', f'file:{serial}', '-no-reboot',
    ], stdout=log, stderr=subprocess.STDOUT)
    connection = None
    try:
        for _ in range(30):
            if qmp.exists():
                break
            if qemu.poll() is not None:
                raise RuntimeError('QEMU exited before creating its QMP socket')
            time.sleep(1)
        else:
            raise RuntimeError('QEMU QMP socket did not appear')
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        connection.settimeout(20)
        connection.connect(str(qmp))
        stream = connection.makefile('rwb', buffering=0)
        json.loads(stream.readline())

        def command(name, arguments=None):
            request = {'execute': name}
            if arguments is not None:
                request['arguments'] = arguments
            stream.write(json.dumps(request).encode() + b'\n')
            while True:
                response = json.loads(stream.readline())
                if 'error' in response:
                    raise RuntimeError(f'{name}: {response["error"]}')
                if 'return' in response:
                    return response['return']

        command('qmp_capabilities')

        def capture(name):
            path = output / f'{name}.ppm'
            command('screendump', {'filename': str(path)})
            screenshot = Image.open(path).convert('RGB')
            screenshot.save(output / f'{name}.png')
            return screenshot

        def double_click(x, y, width, height):
            command('input-send-event', {'events': [
                {'type': 'abs', 'data': {'axis': 'x',
                    'value': round(x * 32767 / (width - 1))}},
                {'type': 'abs', 'data': {'axis': 'y',
                    'value': round(y * 32767 / (height - 1))}},
            ]})
            for _ in range(2):
                command('input-send-event', {'events': [
                    {'type': 'btn',
                     'data': {'down': True, 'button': 'left'}},
                ]})
                time.sleep(0.1)
                command('input-send-event', {'events': [
                    {'type': 'btn',
                     'data': {'down': False, 'button': 'left'}},
                ]})
                time.sleep(0.15)
            time.sleep(2)

        print('Waiting for saved OSTen image to boot...', flush=True)
        time.sleep(int(os.environ.get('OSTEN_BOOT_WAIT_SECONDS', '180')))
        if qemu.poll() is not None:
            raise RuntimeError('QEMU exited during boot')
        command('human-monitor-command', {'command-line': 'sendkey ret'})
        time.sleep(int(os.environ.get('OSTEN_DESKTOP_WAIT_SECONDS', '90')))
        desktop = capture('system-folder-desktop')
        width, height = desktop.size
        double_click(width - 56, 75, width, height)
        capture('system-folder-volume')
        x, y = locate_label(output / 'system-folder-volume.png', 'System', 80)
        print(f'Opening System Folder at ({x}, {y})', flush=True)
        double_click(x, y, width, height)
        screenshot = capture('system-folder')
        folder_image = output / 'system-folder.png'
        # Confirm both rows are visible. OCR reads the short System label as
        # gibberish at 40px, although the artwork itself renders correctly.
        locate_label(folder_image, 'Folder', min_top=60)
        locate_label(folder_image, 'Startup', min_top=180)
        finder_label = locate_label(folder_image, 'Finder', min_top=80)
        verify_artwork(folder_image, 'Finder', finder_label)
        # Native Finder sorts: Control Panels, Extensions, Finder, Fonts,
        # Startup Items, System. Its icon grid is 94px wide x 86px high.
        # Locate System relative to the reliably recognized Finder label.
        system_label = (finder_label[0] - 94, finder_label[1] + 86)
        verify_artwork(folder_image, 'System', system_label)
        # Enlarged view makes 40px icons readable on a GitHub artifact page.
        detail = screenshot.crop((40, 30, min(650, width), min(500, height)))
        detail.resize((detail.width * 2, detail.height * 2),
            Image.Resampling.NEAREST).save(
                output / 'system-folder-detail.png')
        print('Verified Finder and System icons in System Folder', flush=True)
        command('quit')
    finally:
        if connection is not None:
            connection.close()
        if qemu.poll() is None:
            qemu.terminate()
            try:
                qemu.wait(timeout=10)
            except subprocess.TimeoutExpired:
                qemu.kill()
                qemu.wait()
        log.close()
        qmp.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
