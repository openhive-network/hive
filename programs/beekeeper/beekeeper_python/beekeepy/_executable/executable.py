from __future__ import annotations

import os
import signal
import subprocess
import warnings
from typing import TYPE_CHECKING

from beekeepy._executable.streams import StreamsHolder

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger


class Executable:
    def __init__(self, executable_path: Path, working_directory: Path, logger: Logger) -> None:
        if working_directory.exists():
            assert working_directory.is_dir(), "Given path is not pointing to directory"
        else:
            working_directory.mkdir()
        assert executable_path.is_file(), "Given executable path is not pointing to file"
        self.__executable_path = executable_path.absolute()
        self.__process: subprocess.Popen[str] | None = None
        self.__directory = working_directory.absolute()
        self.__logger = logger
        self.__files = StreamsHolder()
        self.__files.set_paths_for_dir(self.__directory)

    def run(
        self,
        *,
        blocking: bool,
        arguments: list[str] | None = None,
        environ: dict[str, str] | None = None,
    ) -> None:
        arguments = arguments or []
        environ = environ or {}

        self.__directory.mkdir(exist_ok=True)
        command: list[str] = [self.__executable_path.as_posix(), *arguments]
        self.__logger.debug(f"Starting {self.__executable_path.name} with arguments: " + " ".join(arguments))

        environment_variables = dict(os.environ)
        environment_variables.update(environ)

        if self.__files.requires_backup():
            self.__files.backup()

        if blocking:
            with self.__files.stdout as stdout, self.__files.stderr as stderr:
                subprocess.run(
                    command,
                    cwd=self.__directory,
                    env=environment_variables,
                    check=True,
                    stdout=stdout,
                    stderr=stderr,
                )
                return

        # Process created here have to exist longer than current scope
        self.__process = subprocess.Popen(  # type: ignore
            command,
            cwd=self.__directory,
            env=environment_variables,
            stdout=self.__files.stdout.open_stream(),
            stderr=self.__files.stderr.open_stream(),
        )

    @property
    def pid(self) -> int:
        assert self.__process is not None
        return self.__process.pid

    def __raise_warning(self) -> None:
        warnings.warn(
            # break because of formatting
            "Process was force-closed with SIGKILL," + "because didn't close before timeout",
            stacklevel=2,
        )

    def close(self) -> None:
        if self.__process is None:
            return

        self.__process.send_signal(signal.SIGINT)
        try:
            return_code = self.__process.wait(timeout=10)
            self.__logger.debug(f"Closed with {return_code} return code")
        except subprocess.TimeoutExpired:
            self.__raise_warning()
            self.__process.kill()
            self.__process.wait()
        self.__process = None
        self.__files.close()

    def is_running(self) -> bool:
        if not self.__process:
            return False

        return self.__process.poll() is None

    def are_logs_contains(self, text: str) -> bool:
        return text in self.__files

    @property
    def woring_dir(self) -> Path:
        return self.__directory
