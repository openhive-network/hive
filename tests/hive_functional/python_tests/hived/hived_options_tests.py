#!/usr/bin/python3

import sys
import tempfile
import json
from pathlib import Path
sys.path.append("../../")
import hive_utils

def assert_eq(x, y):
    assert x == y, "{} != {}".format(x, y)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-hived", dest="hived_path", help = "Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")

    args = parser.parse_args()

    node = None

    if args.hived_path:
        work_dir = tempfile.gettempdir()
        config_file_name = 'config_test.ini'

        node = hive_utils.hive_node.HiveNode(
            args.hived_path,
            work_dir,
            ['--config=' + config_file_name, '--dump-config']
        )

    with node:
        out = node.get_output()
        js = json.loads(out)
        config_path = Path(work_dir) / config_file_name
        assert_eq(Path(js['data-dir']), Path(work_dir))
        assert_eq(Path(js['config']), config_path)
