import subprocess
from pathlib import Path

package_name = Path('.').absolute().parts[-1]

destination = Path(subprocess.check_output(['python3', '-m', 'site', '--user-site'], encoding='utf-8').strip())
destination /= package_name

if destination.exists():
    print(f'You have already installed {package_name}.')
    exit(0)

source = Path('package/test_library/').absolute()
destination.symlink_to(source)

print(f'''You have successfully installed {package_name}.

Details:
  Symlink to [1] created in [2].
  [1] {source}
  [2] {destination}''')
