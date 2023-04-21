import os
import glob
import sys

from pathlib import Path

import test_tools as tt

from shared_tools.cpp_setup import main

def cpp_prepare( setup_name: str = "cpp_setup.py", setup_mode: str = "setup_all", build_lib: Path = "./", build_temp: Path = "./" ):
    tt.logger.info(f"start of cpp preparation...")

    backup = sys.argv
    sys.argv = [f"{setup_name}", f"{setup_mode}", "--build-lib", f"{build_lib}", "--build-temp", f"{build_temp}"]
    main()
    sys.argv = backup

    tt.logger.info(f"cpp preparation finished...")