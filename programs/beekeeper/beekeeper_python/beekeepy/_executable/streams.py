from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, TextIO, cast

from helpy import ContextSync

if TYPE_CHECKING:
    from pathlib import Path


@dataclass
class StreamRepresentation(ContextSync[TextIO]):
    filename: str
    dirpath: Path | None = None
    stream: TextIO | None = None
    _backup_count: int = 0
    _current_filename: str | None = None

    def __get_path(self) -> Path:
        assert self.dirpath is not None, "Path is not specified"
        if self._current_filename is None:
            self._current_filename = self.__next_filename()  # dummy for mypy
        return self.dirpath / self._current_filename

    def __get_stream(self) -> TextIO:
        assert self.stream is not None, "Unable to get stream, as it is not opened"
        return self.stream

    def open_stream(self, mode: str = "wt") -> TextIO:
        assert self.stream is None, "Stream is already opened"
        self.__next_filename()
        self.__ceate_user_friendly_link()
        self.stream = cast(TextIO, self.__get_path().open(mode))
        assert not self.stream.closed, f"Failed to open stream: `{self.stream.errors}`"
        return self.stream

    def close_stream(self) -> None:
        self.__get_stream().close()
        self.stream = None

    def set_path_for_dir(self, dir_path: Path) -> None:
        self.dirpath = dir_path

    def _enter(self) -> TextIO:
        return self.open_stream()

    def _finally(self) -> None:
        self.close_stream()

    def __ceate_user_friendly_link(self) -> None:
        assert self.dirpath is not None, "dirpath is not set"
        user_friendly_link_dst = self.dirpath / f"{self.filename}.log"
        if user_friendly_link_dst.exists() and user_friendly_link_dst.is_symlink():
            user_friendly_link_dst.unlink()
        user_friendly_link_dst.symlink_to(self.__get_path())

    def __next_filename(self) -> str:
        self._current_filename = f"{self.filename}_{self._backup_count}.log"
        self._backup_count += 1
        return self._current_filename

    def __contains__(self, text: str) -> bool:
        if not self.__get_path().exists():
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

    def close(self) -> None:
        self.stdout.close_stream()
        self.stderr.close_stream()

    def __contains__(self, text: str) -> bool:
        return (text in self.stderr) or (text in self.stdout)
