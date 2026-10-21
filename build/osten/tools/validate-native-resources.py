#!/usr/bin/env python3
"""Fast, dependency-free native OSTen resource-index conformance checks.

Run before downloading/building the cross-compiler. Tests documented native
catalogs and their references, not yet full runtime/GUI compliance.
"""
from __future__ import annotations

import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
APPS = ("Finder", "System", "ResourceEditor")
BASE = ROOT / "data" / "osten" / "resources"
errors: list[str] = []


def check(cond: bool, message: str) -> None:
    if not cond:
        errors.append(message)


def parse_manifest(path: pathlib.Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith(("#", ";")):
            continue
        check("=" in line, f"{path}:{number}: invalid key/value")
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        check(bool(re.fullmatch(r"[A-Za-z0-9._-]+", key)),
              f"{path}:{number}: invalid resource key")
        check(key not in result, f"{path}:{number}: duplicate resource {key}")
        result[key] = value
    return result


def parse_index(path: pathlib.Path) -> dict[str, tuple[int, str, str, int, int]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    check(bool(lines) and lines[0] == "OSTEN-RESOURCE-INDEX/1",
          f"{path}: wrong schema version/header")
    output: dict[str, tuple[int, str, str, int, int]] = {}
    ids: set[int] = set()
    for number, row in enumerate(lines[1:], 2):
        if not row or row.startswith("#"):
            continue
        fields = row.split("|")
        check(len(fields) == 6, f"{path}:{number}: expected 6 columns")
        if len(fields) != 6:
            continue
        identifier, kind, key, label, raw_width, raw_height = fields
        try:
            id_value = int(identifier, 0)
            width = int(raw_width)
            height = int(raw_height)
        except ValueError:
            errors.append(f"{path}:{number}: invalid ID/dimensions")
            continue
        check(0 <= id_value <= 0xFFFFFFFF, f"{path}:{number}: ID out of range")
        check(id_value not in ids, f"{path}:{number}: duplicate stable ID")
        ids.add(id_value)
        check(key not in output, f"{path}:{number}: duplicate key {key}")
        check(bool(re.fullmatch("[A-Za-z0-9._-]+", key)),
              f"{path}:{number}: bad key {key}")
        check(bool(re.fullmatch("[A-Z][A-Z0-9]{1,7}", kind)),
              f"{path}:{number}: unrecognized type syntax {kind}")
        check(bool(label) and "|" not in label, f"{path}:{number}: bad label")
        check(0 <= width <= 2048 and 0 <= height <= 2048,
              f"{path}:{number}: bad image dimensions")
        output[key] = (id_value, kind, label, width, height)
    return output


def validate_app(app: str) -> None:
    directory = BASE / app
    values = parse_manifest(directory / "Interface.ostr")
    entries = parse_index(directory / "ResourceIndex.osti")
    artwork = {p.stem for p in directory.glob("*.rgba")}
    catalogued = set(values) | artwork
    check(set(entries) == catalogued,
          f"{app}: index coverage mismatch: missing={sorted(catalogued - set(entries))}; "
          f"unbound={sorted(set(entries) - catalogued)}")
    for name in artwork:
        if name not in entries:
            continue
        _, kind, _, width, height = entries[name]
        check(kind in ("ICON", "IMAG"), f"{app}/{name}: bad artwork type")
        check(width > 0 and height > 0,
              f"{app}/{name}: raw image size not indexed")
        check((directory / (name + ".rgba")).stat().st_size == width * height * 4,
              f"{app}/{name}: indexed image dimensions do not match file")
    for name, (identifier, _, _, width, height) in entries.items():
        if name not in artwork:
            check(width == height == 0,
                  f"{app}/{name}: nonimage has image dimensions")
    print(f"{app}: {len(entries)} typed resources, "
          f"{len(artwork)} artworks, {len(values)} editable manifest keys")


def check_editor_bindings() -> None:
    manifest = parse_manifest(BASE / "ResourceEditor" / "Interface.ostr")
    source = (
        ROOT / "src/apps/osten/resedit/ResourceBrowser.cpp"
    ).read_text(encoding="utf-8")
    accessed = set(re.findall(
        r'BrowserUI\(\)\.(?:String|Rect|Color|Number)\("([^"]+)"', source))
    used = {key for key in accessed if key.startswith("browser.")}
    missing = sorted(used - set(manifest))
    check(not missing, f"ResourceEditor: browser references absent from manifest: {missing}")
    print(f"ResourceEditor: {len(used)} literal browser resource references checked")


def main() -> int:
    for app in APPS:
        validate_app(app)
    check_editor_bindings()
    if errors:
        print("\nFAILED", file=sys.stderr)
        for error in errors:
            print("  " + error, file=sys.stderr)
        return 1
    print("\nIndex schemas and catalog bindings: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
