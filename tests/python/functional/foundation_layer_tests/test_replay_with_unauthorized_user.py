from pathlib import Path
from subprocess import run
import getpass
import grp
import os
import pwd
import sys
import time

import test_tools as tt


def run_python_script_as_user(user: str, path_to_script: Path, args: list[str], cwd: Path) -> None:
    command = [
        'sudo', '-u', user, sys.executable,
        path_to_script.as_posix(), *args
    ]
    run(check=True, args=command, cwd=cwd)
    time.sleep(1.0)


def test_replay_with_unauthorized_user(block_log_empty_430_split: tt.BlockLog):
    path_to_resource_directory = Path(__file__).resolve().parent / "resources"
    path_to_script = path_to_resource_directory / "standalone_beekeeper_by_args.py"


    node = tt.InitNode()
    node.config.block_log_split = 9999

    path =block_log_empty_430_split.block_files[0]

    mode = os.stat(path).st_mode

    permissions = oct(mode & 0o777)

    tt.logger.info(f"permissions file {path}: {permissions}")

    stat_info = os.stat(path)

    # UID i GID
    uid = stat_info.st_uid
    gid = stat_info.st_gid

    owner = pwd.getpwuid(uid).pw_name
    group = grp.getgrgid(gid).gr_name

    current_user = getpass.getuser()
    users = [entry.pw_name for entry in pwd.getpwall()]

    tt.logger.info(f"owner: {owner}")
    tt.logger.info(f"group: {group}")
    tt.logger.info(f"current_user: {current_user}")
    tt.logger.info(f"users: {users}")


    node.run(replay_from=block_log_empty_430_split)
    node.wait_for_block_with_number(10)
    tt.logger.info("FIN")
