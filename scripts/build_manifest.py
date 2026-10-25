"""Publish the exact classic .pak set from GameItems; Python 3, no dependencies."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

CATEGORIES = ("Models", "Armor", "Clothing", "Weapons")
MAX_BYTES = 95 * 1024 * 1024


def build(root: Path, commit: str) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("source_commit must be a full Git commit SHA")
    items = root / "GameItems"
    packs, seen, catalogs = [], set(), set()
    for category in CATEGORIES:
        folder = items / category
        if not folder.is_dir() or folder.is_symlink():
            raise ValueError(f"Missing or linked category directory: {category}")
        for path in sorted(folder.rglob("*")):
            if path.is_symlink():
                raise ValueError(f"Linked content is unsupported: {path.name}")
            if not path.is_file():
                continue
            if path.suffix.lower() in (".utoc", ".ucas"):
                raise ValueError("IoStore containers are unsupported in this first classic .pak workflow")
            if path.suffix.lower() != ".pak":
                continue
            relative = path.relative_to(root).as_posix()
            if not re.fullmatch(r"GameItems/(Models|Armor|Clothing|Weapons)/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.pak", relative):
                raise ValueError(f"Use letters, digits, underscores, and hyphens in pack paths: {relative}")
            key = relative.casefold()
            if key in seen:
                raise ValueError(f"Case-insensitive duplicate pack path: {relative}")
            seen.add(key)
            size = path.stat().st_size
            if not 0 < size <= MAX_BYTES:
                raise ValueError(f"Pack must be nonempty and at most 95 MiB: {relative}")
            descriptor = path.with_suffix(".pack.json")
            if not descriptor.is_file() or descriptor.is_symlink():
                raise ValueError(f"Missing pack entry-point descriptor: {descriptor.name}")
            metadata = json.loads(descriptor.read_text(encoding="utf-8-sig"))
            catalog = metadata.get("catalog", "")
            if metadata.get("schema_version") != 1 or not isinstance(catalog, str) or not re.fullmatch(
                r"/Game/GFExternal/(?:[A-Za-z0-9_]+/)*([A-Za-z0-9_]+)\.\1", catalog
            ):
                raise ValueError(f"Invalid external catalog path in {descriptor.name}")
            if catalog.casefold() in catalogs:
                raise ValueError(f"Duplicate pack catalog: {catalog}")
            catalogs.add(catalog.casefold())
            with path.open("rb") as stream:
                if stream.read(150).startswith(b"version https://git-lfs.github.com/spec/v1"):
                    raise ValueError(f"LFS pointers are not downloadable pack bytes: {relative}")
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            packs.append({"id": hashlib.sha256(relative.encode()).hexdigest()[:24],
                          "path": relative, "size": size, "sha256": digest.hexdigest(), "catalog": catalog})
    return {"schema_version": 1, "project": "GodFall_Clean", "engine": "5.8",
            "platform": "Win64", "source_commit": commit, "packs": packs}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    manifest = build(args.root.resolve(), args.source_commit)
    destination = args.root / "GameItems" / "manifest.json"
    temporary = destination.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temporary.replace(destination)
    print(f"Published manifest for {len(manifest['packs'])} pack(s) at {args.source_commit}")


if __name__ == "__main__":
    main()
