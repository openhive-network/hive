from pathlib import Path
from setuptools import setup, find_packages
from distutils.command.build import build
from shutil import which, rmtree, copyfile
from subprocess import run as run_process
from multiprocessing import cpu_count


class CustomBuild(build):
    binary_name = "wax.cpython-310-x86_64-linux-gnu.so"
    current_dir = Path(__file__).parent.absolute()
    packages_dir = current_dir / "package"
    wax_package_shared_lib = packages_dir / binary_name
    build_dir = current_dir / ".build"
    logs_dir = build_dir / "logs"

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

        self.build_dir.mkdir(exist_ok=True)
        print(f"build will be performed in: {self.build_dir}")
        self.logs_dir.mkdir(exist_ok=True)
        with (self.logs_dir / "stdout.log").open("w") as stdout, (
            self.logs_dir / "stderr.log"
        ).open("w") as stderr:
            print(f"all build logs will be saved to: {self.logs_dir.as_posix()}")
            print(f"configuring with {configure_command}")
            run_process(
                [
                    configure_command,
                    "-S",
                    self.current_dir.as_posix(),
                    "-B",
                    self.build_dir.as_posix(),
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
                cwd=self.build_dir,
            ).check_returncode()
            print("build succeeded")

        output_binary = self.build_dir / self.binary_name
        assert output_binary.exists()
        print(f"copying library from {output_binary} to {self.wax_package_shared_lib}")
        copyfile(output_binary, self.wax_package_shared_lib)
        super().run()


if __name__ == "__main__":
    setup(
        name="wax",
        version="0.0.0",
        packages=[""],
        package_dir={"": "package"},
        package_data={"": [CustomBuild.binary_name, "wax.pyi"]},
        cmdclass={"build": CustomBuild},
    )
