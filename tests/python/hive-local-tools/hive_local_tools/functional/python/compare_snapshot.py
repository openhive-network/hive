from __future__ import annotations

import os
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING, Final

from test_tools.__private.paths_to_executables import BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE

if TYPE_CHECKING:
    from test_tools.__private.snapshot import Snapshot

__all__ = ["compare_snapshots_contents"]

ComparisonDictionaryT = dict[str, dict[str, str]]


def _extract_hex_data_from_file(file_to_parse: Path) -> dict[str, str]:
    hex_word: Final[str] = "HEX"
    lookup_point_phrase: Final[str] = "Data Block"

    result: dict[str, str] = {}

    with file_to_parse.open("rb") as file:
        is_data_block_found = False
        for line in file:
            try:
                line = line.decode().strip()
            except UnicodeDecodeError:
                continue
            if not is_data_block_found:
                if line.startswith(lookup_point_phrase):
                    is_data_block_found = True
                continue

            if line.startswith(hex_word):
                line = line[len(hex_word) :]

                key, value = line.split(":")
                result[key.strip()] = value.strip()
    return result


def _find_sst_files_in_directory(directory: Path) -> list[Path]:
    return list(directory.rglob("*.sst"))


def _get_path_to_sst_dump() -> Path:
    path = os.environ.get("SST_DUMP_PATH")
    if path is None:
        path = os.environ.get(BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE)
        if path is not None:
            path = Path(path) / "libraries" / "vendor" / "rocksdb" / "tools" / "sst_dump"
    else:
        path = Path(path)

    assert path is not None, f"SST_DUMP_PATH nor {BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE} is not set"
    return path


def _extract_data_from_sst_file(sst_file_to_dump: Path) -> dict[str, str]:
    sst_dump_exec = _get_path_to_sst_dump()
    subprocess.run(
        [sst_dump_exec.as_posix(), f"--file={sst_file_to_dump.as_posix()}", "--command=raw"],
        cwd=sst_file_to_dump.parent,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    dump_file = sst_file_to_dump.with_name(f"{sst_file_to_dump.stem}_dump.txt")
    return _extract_hex_data_from_file(dump_file)


def _convert_path_to_identifier(path: Path) -> str:
    return f"{path.parent.name}/{path.name}"


def _create_comparable_snapshot(directory: Path) -> ComparisonDictionaryT:
    result: ComparisonDictionaryT = {}
    for sst_path in _find_sst_files_in_directory(directory):
        key = _convert_path_to_identifier(sst_path)
        value = _extract_data_from_sst_file(sst_path)
        result[key] = value
    return result


def compare_snapshots_contents(snapshot_1: Snapshot, snapshot_2: Snapshot) -> None:
    snap1 = _create_comparable_snapshot(snapshot_1.get_path())
    snap2 = _create_comparable_snapshot(snapshot_2.get_path())

    # Number of *.SST files can be different if original data is held in RocksDB.
    # It applies to AH data and comments in RocksDB.

    # assert len(snap1) == len(snap2), "Amount of files does not match between snapshots."
    # assert set(snap1.keys()) == set(snap2.keys()), "Files in both snapshots does not match"

    smaller_snap = None
    bigger_snap = None
    if len(snap1) > len(snap2):
        smaller_snap = snap2
        bigger_snap = snap1
    else:
        smaller_snap = snap1
        bigger_snap = snap2

    for file_name, file_content_1 in smaller_snap.items():
        file_content_2 = bigger_snap[file_name]
        assert len(file_content_1) == len(file_content_2), f"Amount of entities does not match in `{file_name}`"
        assert set(file_content_1.keys()) == set(file_content_2.keys()), f"Keys in `{file_name}` does not match"

        for key, value_1 in file_content_1.items():
            value_2 = file_content_2[key]
            assert value_1 == value_2, f"In file `{file_name}`, for key `{key}` values does not match"
