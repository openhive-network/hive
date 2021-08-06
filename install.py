#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path


def check_paths_to_executables():
    from test_tools import paths_to_executables
    paths = paths_to_executables.get_paths_in_use()

    if any([path is None for executable, path in paths.items()]):
        print('\nSome paths to executables are missing:')
        for executable, path in paths.items():
            print(f'{executable}: {path if path is not None else "not set"}')

        print()
        paths_to_executables.print_configuration_hint()
    else:
        print('\nPaths are configured correctly:')
        for executable, path in paths.items():
            print(f'{executable}: {path}')


def install():
    this_script_directory = Path(__file__).parent.absolute()
    package_name = this_script_directory.parts[-1]

    destination = Path(subprocess.check_output(['python3', '-m', 'site', '--user-site'], encoding='utf-8').strip())
    destination /= package_name

    if destination.exists():
        print(f'You have already installed {package_name}.')
        check_paths_to_executables()
        sys.exit(0)

    source = this_script_directory / 'package/test_tools/'
    destination.symlink_to(source)

    print(
        f'You have successfully installed {package_name}.\n'
        f'\n'
        f'Details:\n'
        f'  Symlink to [1] created in [2].\n'
        f'  [1] {source}\n'
        f'  [2] {destination}'
    )

    check_paths_to_executables()


if __name__ == '__main__':
    install()
