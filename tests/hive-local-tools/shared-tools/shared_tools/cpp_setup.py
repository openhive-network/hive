from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from Cython.Build import cythonize

from pathlib import Path
import glob, os

def get_sources(path: Path = "./", patterns: list[str] = [ "*.cpp", "*.pyx" ]):
    files = []
    os.chdir(path)
    for pattern in patterns:
        for file in glob.glob(pattern):
            files.append(file)
    return files

sources = get_sources()
print(f"found sources: {sources}")

language            = "c++"
extra_compile_args  = ["-std=c++17"]

extensions = [Extension(
                name                = "cpp_interface",
                language            = language,
                extra_compile_args  = extra_compile_args,
                sources             = sources
            )]

class BuildExt(build_ext):
    def build_extensions(self):
        super().build_extensions()

def main():
    cython_kwargs = {
        "nthreads" : 1,
    }

    setup (
            ext_modules = cythonize(extensions, **cython_kwargs),
            cmdclass = {'setup_all': BuildExt},
          )


if __name__ == "__main__":
    main()
