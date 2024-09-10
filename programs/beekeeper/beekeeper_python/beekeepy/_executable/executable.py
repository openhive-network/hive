from __future__ import annotations

import os
import signal
import subprocess
import time
import warnings
from abc import abstractmethod
from typing import TYPE_CHECKING, Any, Generic, TypeVar

from beekeepy._executable.streams import StreamsHolder
from helpy._interfaces.config import Config
from helpy._interfaces.context import ContextSync

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger


class Closeable:
    @abstractmethod
    def close(self) -> None:
        ...


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


class Executable(Closeable, Generic[ConfigT]):
    def __init__(self, executable_path: Path, working_directory: Path, logger: Logger) -> None:
        if working_directory.exists():
            assert working_directory.is_dir(), "Given path is not pointing to directory"
        else:
            working_directory.mkdir()
        assert executable_path.is_file(), "Given executable path is not pointing to file"
        self.__executable_path = executable_path.absolute()
        self.__process: subprocess.Popen[str] | None = None
        self.__directory = working_directory.absolute()
        self._logger = logger
        self.__files = StreamsHolder()
        self.__files.set_paths_for_dir(self.__directory)
        self.__config: ConfigT = self._construct_config()

    @property
    def pid(self) -> int:
        assert self.__process is not None, "Process is not running, nothing to return"
        return self.__process.pid

    @property
    def woring_dir(self) -> Path:
        return self.__directory

    @property
    def config(self) -> ConfigT:
        return self.__config

    def run(
        self,
        *,
        blocking: bool,
        arguments: list[str] | None = None,
        environ: dict[str, str] | None = None,
    ) -> AutoCloser:
        command, environment_variables = self.__prepare(arguments=arguments, environ=environ)

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
                return AutoCloser(None)

        # Process created here have to exist longer than current scope
        self.__process = subprocess.Popen(  # type: ignore
            command,
            cwd=self.__directory,
            env=environment_variables,
            stdout=self.__files.stdout.open_stream(),
            stderr=self.__files.stderr.open_stream(),
        )

        return AutoCloser(self)

    def run_and_get_output(self, arguments: list[str], environ: dict[str, str] | None = None) -> str:
        command, environment_variables = self.__prepare(arguments=arguments, environ=environ)
        result = subprocess.check_output(command, stderr=subprocess.STDOUT, env=environment_variables)
        return result.decode("ascii").strip()

    def __prepare(
        self, arguments: list[str] | None, environ: dict[str, str] | None
    ) -> tuple[list[str], dict[str, str]]:
        arguments = arguments or []
        environ = environ or {}

        self.__directory.mkdir(exist_ok=True)
        command: list[str] = [self.__executable_path.as_posix(), *arguments]
        self._logger.debug(
            f"Starting {self.__executable_path.name} in {self.woring_dir} with arguments: " + " ".join(command)
        )

        environment_variables = dict(os.environ)
        environment_variables.update(environ)

        if self.__files.requires_backup():
            self.__files.backup()

        self.config.save(self.woring_dir)

        return command, environment_variables

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
            self._logger.debug(f"Closed with {return_code} return code")
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

    @abstractmethod
    def _construct_config(self) -> ConfigT:
        ...

    def generate_default_config(self) -> ConfigT:
        path_to_config = self.woring_dir / (Config.DEFAULT_FILE_NAME + ".tmp")
        self.run(blocking=True, arguments=["--dump-config", "-c", path_to_config.as_posix()])
        return self.config.load(path_to_config)

    def get_help_text(self) -> str:
        return self.run_and_get_output(arguments=["--help"])

    def version(self) -> str:
        return self.run_and_get_output(arguments=["--version"])
