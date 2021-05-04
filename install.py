import subprocess
from pathlib import Path

this_script_directory = Path(__file__).parent.absolute()
package_name = this_script_directory.parts[-1]

destination = Path(subprocess.check_output(['python3', '-m', 'site', '--user-site'], encoding='utf-8').strip())
destination /= package_name

if destination.exists():
    print(f'You have already installed {package_name}.')
    exit(0)

source = this_script_directory / 'package/test_library/'
destination.symlink_to(source)

print(f'''You have successfully installed {package_name}.

Details:
  Symlink to [1] created in [2].
  [1] {source}
  [2] {destination}''')
