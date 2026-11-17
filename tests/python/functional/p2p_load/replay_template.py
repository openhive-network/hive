"""
Replayed node state kept on tmpfs and copied into each run's nodes instead of replaying the block log on every node.

A template is the `blockchain/` directory of a node that replayed the fixture and exited before syncing. It is keyed
by everything that decides what that directory holds: the hived binary's content, the fixture's block log and chain
spec, the node's plugins, its shared file size and its block log layout. A new binary or fixture gets a new key, so
the next run replays again; nothing is ever reused across keys.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import time
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

# bumped when what a template holds or how it is keyed changes, so older templates are never picked up
_FORMAT = 1
_STATE_DIR = "blockchain"
_MARKER = "template.json"
_PARTIAL_PREFIX = ".partial-"
_CHUNK = 1 << 20
_DIGESTS: dict[tuple[Path, int, int], str] = {}


def _file_digest(path: Path) -> str:
    """sha256 of the file's content, computed once per content identity (path, size, modification time)."""
    stat = path.stat()
    identity = (path, stat.st_size, stat.st_mtime_ns)
    if identity not in _DIGESTS:
        digest = hashlib.sha256()
        with path.open("rb") as file:
            while chunk := file.read(_CHUNK):
                digest.update(chunk)
        _DIGESTS[identity] = digest.hexdigest()
    return _DIGESTS[identity]


def fixture_digest(block_log_dir: Path) -> str:
    """
    Digest of what a replay reads from the fixture directory: its block log files and alternate-chain-spec.json.

    Block log artifacts are left out: they are derived from the block log, and replays may regenerate them.
    """
    files = sorted(
        path
        for path in block_log_dir.iterdir()
        if path.is_file()
        and (
            path.name == "alternate-chain-spec.json"
            or (path.name.startswith("block_log") and path.suffix != ".artifacts")
        )
    )
    if not any(path.name.startswith("block_log") for path in files):
        raise FileNotFoundError(f"{block_log_dir} has no block_log files")
    digest = hashlib.sha256()
    for path in files:
        digest.update(f"{path.name}\0{_file_digest(path)}\0".encode())
    return digest.hexdigest()


def template_key(
    hived: Path, fixture: str, plugins: Iterable[str], shared_file_size: str, block_log_split: object
) -> str:
    """
    The key of the state `hived` leaves after replaying the fixture (`fixture_digest`) with this node configuration.

    Plugins are a set: their order in the config does not change the state.
    """
    ingredients = {
        "format": _FORMAT,
        "hived": _file_digest(hived.resolve()),
        "fixture": fixture,
        "plugins": sorted(set(plugins)),
        "shared_file_size": str(shared_file_size),
        "block_log_split": str(block_log_split),
    }
    return hashlib.sha256(json.dumps(ingredients, sort_keys=True).encode()).hexdigest()[:32]


def copy_state(source: Path, destination: Path) -> None:
    """
    Copy a state directory, keeping the shared memory file sparse (4 GiB apparent, a fraction of it used).

    shutil would write out the holes; tmpfs has no reflinks, so `cp --sparse=always` is the cheapest copy there is.
    """
    subprocess.run(["cp", "-a", "--sparse=always", str(source), str(destination)], check=True)


def _mark_used(marker: Path) -> None:
    # set explicitly: the kernel stamps writes with a coarse clock, which could order quick successive uses wrongly
    now = time.time()
    os.utime(marker, (now, now))


class TemplateStore:
    """Templates under one root directory, one subdirectory per key; the caller holds the host lock."""

    def __init__(self, root: Path) -> None:
        self._root = root

    def _dir(self, key: str) -> Path:
        return self._root / key

    def find(self, key: str) -> Path | None:
        """Return the template's state directory and mark it used, or None when there is no complete template."""
        marker = self._dir(key) / _MARKER
        if not marker.is_file():
            return None
        _mark_used(marker)
        return self._dir(key) / _STATE_DIR

    def save(self, key: str, state_dir: Path, info: dict[str, Any]) -> None:
        """
        Store a copy of `state_dir` (the node that made it must have stopped) under `key` with `info` beside it.

        Built under a partial name and renamed when complete, so an interrupted save leaves no template behind.
        """
        partial = self._root / f"{_PARTIAL_PREFIX}{key}"
        if partial.exists():
            shutil.rmtree(partial)
        partial.mkdir(parents=True)
        copy_state(state_dir, partial / _STATE_DIR)
        (partial / _MARKER).write_text(json.dumps({**info, "key": key, "created": time.time()}, indent=1))
        _mark_used(partial / _MARKER)
        if self._dir(key).exists():
            shutil.rmtree(self._dir(key))
        partial.rename(self._dir(key))

    def prune(self, keep: int, protect: Iterable[str] = ()) -> list[str]:
        """
        Remove all but the `keep` most recently used templates (never one in `protect`) and any partial leftovers.

        Return the removed keys.
        """
        protected = set(protect)
        complete: list[tuple[float, str]] = []
        for entry in self._root.iterdir() if self._root.is_dir() else []:
            if entry.name.startswith(_PARTIAL_PREFIX):
                shutil.rmtree(entry)
            elif (entry / _MARKER).is_file():
                complete.append(((entry / _MARKER).stat().st_mtime, entry.name))
        complete.sort(reverse=True)
        kept = sum(1 for _, key in complete if key in protected)
        removed = []
        for _, key in complete:
            if key in protected:
                continue
            if kept < keep:
                kept += 1
                continue
            shutil.rmtree(self._dir(key))
            removed.append(key)
        return removed
