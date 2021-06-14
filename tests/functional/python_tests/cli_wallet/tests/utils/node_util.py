import sys
sys.path.append("../../../../")
from tests.utils.cmd_args   import args
from tests.utils.logger     import log
sys.path.append("../../")
import hive_utils


def start_node(**kwargs):
    node=None
    if args.hive_path:
        log.info("Running hived via {} in {} with config {}".format(args.hive_path, 
            args.hive_working_dir, 
            args.hive_config_path)
        )
        
        node = hive_utils.hive_node.HiveNodeInScreen(
            args.hive_path, 
            args.hive_working_dir, 
            args.hive_config_path, **kwargs
        )
    node.run_hive_node(["--enable-stale-production"])
    return node