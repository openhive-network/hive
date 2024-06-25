from __future__ import annotations

from dataclasses import dataclass, field
from shutil import move
from typing import TYPE_CHECKING, TextIO, cast

from helpy import ContextSync

if TYPE_CHECKING:
    from pathlib import Path


@dataclass
class StreamRepresentation(ContextSync[TextIO]):
    filename: str
    path: Path | None = None
    stream: TextIO | None = None
    _backup_count: int = 0

    def __get_path(self) -> Path:
        assert self.path is not None, "Path is not specified"
        return self.path

    def __get_stream(self) -> TextIO:
        assert self.stream is not None, "Unable to get stream, as it is not opened"
        return self.stream

    def backup(self) -> None:
        """Can be called only when streams are closed. No support for backng up opened files."""
        path = self.__get_path()
        assert self.stream is None, "Cannot back up opened file"
        move(path, path.with_name(f"{self.filename}_{self._backup_count}.log"))
        self._backup_count += 1

    def open_stream(self, mode: str = "wt") -> TextIO:
        assert self.stream is None, "Stream is already opened"
        self.stream = cast(TextIO, self.__get_path().open(mode))
        assert not self.stream.closed, f"Failed to open stream: `{self.stream.errors}`"
        return self.stream

    def close_stream(self) -> None:
        self.__get_stream().close()
        self.stream = None

    def set_path_for_dir(self, dir_path: Path) -> None:
        self.path = dir_path / f"{self.filename}.log"

    def _enter(self) -> TextIO:
        return self.open_stream()

    def _finally(self) -> None:
        self.close_stream()

    def __contains__(self, text: str) -> bool:
        if self.path is None:
            return False

        with self.open_stream("rt") as file:
            for line in file:
                if text in line:
                    return True
        return False


@dataclass
class StreamsHolder:
    stdout: StreamRepresentation = field(default_factory=lambda: StreamRepresentation("stdout"))
    stderr: StreamRepresentation = field(default_factory=lambda: StreamRepresentation("stderr"))

    def set_paths_for_dir(self, dir_path: Path) -> None:
        self.stdout.set_path_for_dir(dir_path)
        self.stderr.set_path_for_dir(dir_path)

    def backup(self) -> None:
        """Can be called only when streams are closed. No support for backng up opened files."""
        self.stdout.backup()
        self.stderr.backup()

    def close(self) -> None:
        self.stdout.close_stream()
        self.stderr.close_stream()

    def requires_backup(self) -> bool:
        if self.stderr.path is None or self.stdout.path is None:
            return False
        return self.stdout.path.exists() or self.stderr.path.exists()

    def __contains__(self, text: str) -> bool:
        return (text in self.stderr) or (text in self.stdout)
