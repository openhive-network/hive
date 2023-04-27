from pathlib import Path
from setuptools import setup
from distutils.command.build import build
from shutil import which, rmtree, copyfile
from subprocess import run as run_process
from multiprocessing import cpu_count


class CustomBuild(build):
    BINARY_NAME = "example.cpython-310-x86_64-linux-gnu.so"

    def run(self) -> None:
        configure_command = which("cmake")
        assert configure_command is not None, "cannot find cmake"
        configure_args = ["-GNinja"]
        build_command = which("ninja")
        if build_command is None:
            build_command = which("make")
            assert build_command is not None, "cannot find neither ninja nor make"
            print(f"cannot find ninja, using {build_command} instead")
            configure_args = []

        current_dir = Path(".").absolute()
        build_dir = current_dir / ".build"
        logs_dir = build_dir / "logs"

        if build_dir.exists():
            print(f"removing old build dir {build_dir}")
            rmtree(build_dir)
        build_dir.mkdir()
        print(f"build will be performed in: {build_dir}")
        logs_dir.mkdir()
        with (logs_dir / "stdout.log").open("w") as stdout, (
            logs_dir / "stderr.log"
        ).open("w") as stderr:
            print(f"all build logs will be saved to: {logs_dir.as_posix()}")
            print(f"configuring with {configure_command}")
            run_process(
                [
                    configure_command,
                    "-S",
                    current_dir.as_posix(),
                    "-B",
                    build_dir.as_posix(),
                ]
                + configure_args,
                stdout=stdout,
                stderr=stderr,
            ).check_returncode()

            print(f"building with {build_command}")
            run_process(
                [build_command, "-j", f"{cpu_count()}"],
                stdout=stdout,
                stderr=stderr,
                cwd=build_dir,
            ).check_returncode()
            print("build succeeded")

        output_binary = build_dir / self.BINARY_NAME
        assert output_binary.exists()
        dest_for_binary = current_dir / self.BINARY_NAME
        print(f"copying library from {output_binary} to {dest_for_binary}")
        copyfile(output_binary, dest_for_binary)
        print("removing build directory after successful build")
        rmtree(build_dir)
        super().run()


if __name__ == "__main__":
    current_dir = Path(__file__).parent.absolute()
    setup(
        name="wax",
        version="0.0.0",
        author="Mariusz Trela",
        author_email="mtrela@syncad.com",
        cmdclass={"build": CustomBuild},
        package_data={"": [(current_dir / CustomBuild.BINARY_NAME).as_posix(), (current_dir / "example.pyx").as_posix()]},
    )
