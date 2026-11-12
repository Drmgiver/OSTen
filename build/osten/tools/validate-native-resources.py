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


def validate_composed_dialog(app: str, prefix: str) -> None:
    """Validate packaged default UI schemas; owners may change them at runtime."""
    values = parse_manifest(BASE / app / "Interface.ostr")
    index = parse_index(BASE / app / "ResourceIndex.osti")
    order = values.get(prefix + ".controls", "").split(",")
    available_types = {"text", "label", "button", "list", "text_area",
                       "checkbox", "slider"}
    if app == "ResourceEditor":
        source = (
            ROOT / "src/apps/osten/resedit/ResourceEditor.cpp"
        ).read_text(encoding="utf-8")
        available_types.update(re.findall(r'factories\["([A-Za-z0-9._-]+)"\]', source))
    check(bool(order) and all(order),
          f"{app}/{prefix}: missing or empty declared controls")
    check(len(order) == len(set(order)),
          f"{app}/{prefix}: duplicate control IDs")
    for ident in order:
        stem = f"{prefix}.control.{ident}."
        kind = values.get(stem + "type")
        check(kind in available_types,
              f"{app}/{prefix}/{ident}: unsupported declared control {kind}")
        frameField = values.get(stem + "frame_key") or (stem + "frame")
        frame = values.get(frameField, "")
        pieces = frame.split(",")
        try:
            coords = tuple(float(value) for value in pieces)
            check(len(coords) == 4 and coords[0] < coords[2]
                  and coords[1] < coords[3],
                  f"{app}/{prefix}/{ident}: bad frame {frame}")
        except ValueError:
            errors.append(f"{app}/{prefix}/{ident}: non-numeric frame {frame}")
        check(stem + "type" in index,
              f"{app}/{prefix}/{ident}: no indexed type resource")
        check(frameField in index,
              f"{app}/{prefix}/{ident}: no indexed frame resource")
        if values.get(stem + "frame_key"):
            check(stem + "frame_key" in index,
                  f"{app}/{prefix}/{ident}: unindexed frame alias")
        captionKey = values.get(stem + "caption_key", "")
        if captionKey:
            check(captionKey in values,
                  f"{app}/{prefix}/{ident}: caption key absent {captionKey}")
        action = values.get(stem + "action", "")
        if action:
            check(bool(re.fullmatch(r"[A-Za-z0-9._-]+", action)),
                  f"{app}/{prefix}/{ident}: invalid action identity {action}")
    print(f"{app}: {prefix} — {len(order)} structured controls verified")


def validate_global_menu(app: str, supported_actions: set[str]) -> None:
    """Check built-in menu models have stable, dispatched action identities."""
    values = parse_manifest(BASE / app / "Interface.ostr")
    index = parse_index(BASE / app / "ResourceIndex.osti")
    order = values.get("menu.order", "").split(",")
    check(bool(order) and all(order),
          f"{app}: native menu publisher has no ordered menu sections")
    check(len(order) == len(set(order)),
          f"{app}: repeated top-level menu section")
    for section in order:
        check(bool(re.fullmatch("[A-Za-z0-9._-]+", section)),
              f"{app}: invalid section {section}")
        for key in ("menu." + section, "menu.title." + section):
            check(key in values and key in index,
                  f"{app}: missing indexed menu resource {key}")
        actions = values.get("menu." + section, "").split(",")
        seen = set()
        for action in actions:
            if action == "separator":
                continue
            check(bool(re.fullmatch("[A-Za-z0-9._-]+", action)),
                  f"{app}: invalid menu action {action}")
            check(action not in seen,
                  f"{app}/{section}: duplicate action {action}")
            seen.add(action)
            check(action in supported_actions,
                  f"{app}: published action has no implemented dispatch: {action}")
            key = "menu.item." + action
            check(key in values and key in index,
                  f"{app}: missing indexed label for {action}")
    shortcuts: dict[str, str] = {}
    for key, value in values.items():
        if not key.startswith("shortcut."):
            continue
        check(len(value) <= 1,
              f"{app}/{key}: shortcuts must be single characters")
        if value:
            previous = shortcuts.setdefault(value.upper(), key)
            check(previous == key,
                  f"{app}: shortcut collision {previous} and {key}")
    print(f"{app}: {len(order)} globally published native menu sections checked")


def validate_strip_order() -> None:
    values = parse_manifest(BASE / "System" / "Interface.ostr")
    modules = values.get("strip.module.order", "").split(",")
    for module in modules:
        if not module:
            continue
        check(bool(re.fullmatch(r"[A-Za-z0-9._-]{1,128}", module)),
              f"System: invalid Control Strip resource ID {module}")
    # Deliberately allow empty/subset/repeated modules. The Owner controls
    # their visibility, even when the entire row is removed.
    print("System: Owner-controlled Control Strip module order checked")


def check_editor_status_resources() -> None:
    """ResEdit warning/error/status strings are required editable resources."""
    values = parse_manifest(BASE / "ResourceEditor" / "Interface.ostr")
    index = parse_index(BASE / "ResourceEditor" / "ResourceIndex.osti")
    source = (
        ROOT / "src/apps/osten/resedit/ResourceEditor.cpp"
    ).read_text(encoding="utf-8")
    refs = set(re.findall(
        r'EditorResources\(\)\.String\("(editor\.(?:status|alert)\.[^"]+)"',
        source))
    missing = sorted(name for name in refs
                     if name not in values or name not in index)
    check(not missing,
          f"ResourceEditor: missing owner-editable message resources {missing}")
    print(f"ResourceEditor: {len(refs)} status/alert strings indexed")


def main() -> int:
    for app in APPS:
        validate_app(app)
    check_editor_bindings()
    check_editor_status_resources()
    validate_composed_dialog("Finder", "dialog.rename")
    validate_composed_dialog("Finder", "dialog.info")
    validate_composed_dialog("System", "notification.center")
    validate_composed_dialog("ResourceEditor", "browser.types")
    validate_composed_dialog("ResourceEditor", "browser.entries")
    validate_composed_dialog("ResourceEditor", "editor.inspector")
    validate_global_menu("Finder", {
        "new_folder", "open", "rename", "get_info", "close",
        "select_all", "view_icons", "view_list", "arrange_name",
        "arrange_kind", "clean_up", "empty_trash", "toggle_popup",
        "system_folder_access",
    })
    validate_global_menu("ResourceEditor", {
        "open", "refresh", "quit",
    })
    validate_strip_order()
    if errors:
        print("\nFAILED", file=sys.stderr)
        for error in errors:
            print("  " + error, file=sys.stderr)
        return 1
    print("\nIndex schemas and catalog bindings: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
