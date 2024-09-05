from __future__ import annotations

import os
import signal
import subprocess
import warnings
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Any, Generic, TypeVar

from beekeepy._executable.arguments.arguments import Arguments
from beekeepy._executable.streams import StreamsHolder
from helpy._interfaces.config import Config
from helpy._interfaces.context import ContextSync

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger


class Closeable(ABC):
    @abstractmethod
    def close(self) -> None: ...


class AutoCloser(ContextSync[None]):
    def __init__(self, obj_to_close: Closeable | None, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.__obj_to_close = obj_to_close

    def _enter(self) -> None:
        return None

    def _finally(self) -> None:
        if self.__obj_to_close is not None:
            self.__obj_to_close.close()


ConfigT = TypeVar("ConfigT", bound=Config)
ArgumentT = TypeVar("ArgumentT", bound=Arguments)


class Executable(Closeable, Generic[ConfigT, ArgumentT]):
    def __init__(self, executable_path: Path, working_directory: Path, logger: Logger) -> None:
        if working_directory.exists():
            assert working_directory.is_dir(), "Given path is not pointing to directory"
        else:
            working_directory.mkdir()
        assert executable_path.is_file(), "Given executable path is not pointing to file"
        self.__executable_path = executable_path.absolute()
        self.__process: subprocess.Popen[str] | None = None
        self.__working_directory = working_directory.absolute()
        self._logger = logger
        self.__files = StreamsHolder()
        self.__files.set_paths_for_dir(self.__working_directory)
        self.__config: ConfigT = self._construct_config()
        self.__arguments: ArgumentT = self._construct_arguments()

    @property
    def pid(self) -> int:
        assert self.__process is not None, "Process is not running, nothing to return"
        return self.__process.pid

    @property
    def working_directory(self) -> Path:
        return self.__working_directory

    @property
    def config(self) -> ConfigT:
        return self.__config

    def run(
        self,
        *,
        blocking: bool,
        arguments: ArgumentT | None = None,
        environ: dict[str, str] | None = None,
        propagate_sigint: bool = True,
    ) -> AutoCloser:
        command, environment_variables = self.__prepare(arguments=arguments, environ=environ)

        if blocking:
            with self.__files.stdout as stdout, self.__files.stderr as stderr:
                subprocess.run(
                    command,
                    cwd=self.__working_directory,
                    env=environment_variables,
                    check=True,
                    stdout=stdout,
                    stderr=stderr,
                )
                return AutoCloser(None)

        # Process created here have to exist longer than current scope
        self.__process = subprocess.Popen(  # type: ignore
            command,
            cwd=self.__working_directory,
            env=environment_variables,
            stdout=self.__files.stdout.open_stream(),
            stderr=self.__files.stderr.open_stream(),
            preexec_fn=(os.setpgrp if not propagate_sigint else None),  # noqa: PLW1509 create new process group, so signals won't be passed to child process
        )

        return AutoCloser(self)

    def run_and_get_output(
        self, arguments: ArgumentT, environ: dict[str, str] | None = None, timeout: float | None = None
    ) -> str:
        command, environment_variables = self.__prepare(arguments=arguments, environ=environ)
        result = subprocess.check_output(command, stderr=subprocess.STDOUT, env=environment_variables, timeout=timeout)
        return result.decode().strip()

    def __prepare(
        self, arguments: ArgumentT | None, environ: dict[str, str] | None
    ) -> tuple[list[str], dict[str, str]]:
        arguments = arguments or self.__arguments
        environ = environ or {}

        self.__working_directory.mkdir(exist_ok=True)
        command: list[str] = [self.__executable_path.as_posix(), *(arguments.process())]
        self._logger.debug(
            f"Starting {self.__executable_path.name} in {self.working_directory} with arguments: " + " ".join(command)
        )

        environment_variables = dict(os.environ)
        environment_variables.update(environ)
        self.config.save(self.working_directory)

        return command, environment_variables

    def __raise_warning_if_timeout_on_close(self) -> None:
        warnings.warn(
            # break because of formatting
            "Process was force-closed with SIGKILL, because didn't close before timeout",
            stacklevel=2,
        )

    def detach(self) -> None:
        if self.__process is None:
            return

        self.__process = None
        self.__files.close()

    def close(self, timeout_secs: float = 10.0) -> None:
        if self.__process is None:
            return

        self.__process.send_signal(signal.SIGINT)
        try:
            return_code = self.__process.wait(timeout=timeout_secs)
            self._logger.debug(f"Closed with {return_code} return code")
        except subprocess.TimeoutExpired:
            self.__raise_warning_if_timeout_on_close()
            self.__process.kill()
            self.__process.wait()
        self.__process = None
        self.__files.close()
        self.__warn_if_pid_files_exists()

    def __warn_if_pid_files_exists(self) -> None:
        if self.__pid_files_exists():
            warnings.warn(
                f"PID file has not been removed, malfunction may occur. Working directory: {self.working_directory}",
                stacklevel=2,
            )

    def __pid_files_exists(self) -> bool:
        return len(list(self.working_directory.glob("*.pid"))) > 0

    def is_running(self) -> bool:
        if not self.__process:
            return False

        return self.__process.poll() is None

    def log_has_phrase(self, text: str) -> bool:
        return text in self.__files

    @abstractmethod
    def _construct_config(self) -> ConfigT: ...

    @abstractmethod
    def _construct_arguments(self) -> ArgumentT: ...

    def generate_default_config(self) -> ConfigT:
        path_to_config = self.working_directory / (Config.DEFAULT_FILE_NAME)
        self.run(blocking=True, arguments=self.__arguments.just_dump_config())
        temp_path_to_file = path_to_config.rename(Config.DEFAULT_FILE_NAME + ".tmp")
        return self.config.load(temp_path_to_file)

    def get_help_text(self) -> str:
        return self.run_and_get_output(arguments=self.__arguments.just_get_help())

    def version(self) -> str:
        return self.run_and_get_output(arguments=self.__arguments.just_get_version())
