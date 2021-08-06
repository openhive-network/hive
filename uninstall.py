#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path


def uninstall():
    this_script_directory = Path(__file__).parent.absolute()
    package_name = this_script_directory.parts[-1]

    destination = Path(subprocess.check_output(['python3', '-m', 'site', '--user-site'], encoding='utf-8').strip())
    destination /= package_name

    if not destination.exists():
        print(f'{package_name} aren\'t installed.')
        sys.exit(0)

    destination.unlink()
    print(f'You have successfully uninstalled {package_name}.')


if __name__ == '__main__':
    uninstall()
