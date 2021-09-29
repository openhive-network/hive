## Paths to executables

TestTools contains module which handles paths to executables using by TestTools (like _hived_, _cli_wallet_, and so on...). You can configure which executables should be used in following ways:
1. set directly in script
2. passed as command line arguments
3. environment variables
4. executable installation

If you have paths configured in multiple ways (i.e. you have installed _hived_ and also passed its path as command line argument), they will be used with its priorities. Priorities are same as in list above; path set directly in script is the most important, less important is path passed as command line argument and so on. So in example mentioned earlier, installed _hived_ will be ignored and _hived_ path passed as command line argument will be used.

### Set paths directly in script

You can set path to executable directly in your script like on example below:
```python
from test_tools import paths_to_executables
paths_to_executables.set_path_of('hived', '/home/dev/hive/programs/hived/hived')
```

### Pass paths as command line arguments

To pass path as command line argument you have to run your script with one of following parameters:
| Executable           | Argument name               |
| -------------------- | --------------------------- |
| All executables      | `--build-root-path`         |
| _hived_              | `--hived-path`              |
| _cli_wallet_         | `--cli-wallet-path`         |
| _get_dev_key_        | `--get-dev-key-path`        |
| _truncate_block_log_ | `--truncate-block-log-path` |

Example run with all executables from selected path:
```bash
python3 my_script.py --build-root-path="/home/dev/hive/build"
```

Example run with custom _hived_ path:
```bash
python3 my_script.py --hived-path="/home/dev/hive/build/programs/hived/hived"
```

### Define paths as environment variables

To define path using environment variables you have to use following variable names:
| Executable           | Variable name             |
| -------------------- | ------------------------- |
| All executables      | `HIVE_BUILD_ROOT_PATH`    |
| _hived_              | `HIVED_PATH`              |
| _cli_wallet_         | `CLI_WALLET_PATH`         |
| _get_dev_key_        | `GET_DEV_KEY_PATH`        |
| _truncate_block_log_ | `TRUNCATE_BLOCK_LOG_PATH` |

Example run with all executables from selected path:
```bash
HIVE_BUILD_ROOT_PATH="/home/dev/hive/build/" python3 my_script.py
```

Example run with custom _hived_ path:
```bash
HIVED_PATH="/home/dev/hive/build/programs/hived/hived" python3 my_script.py
```

### Install executables

Installed executables will be automatically detected.
