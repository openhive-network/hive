from pathlib import Path
import re
import subprocess
import time

from test_tools import communication, logger, Hivemind, World, Wallet
def test_hivemind_start_sync(world: World):
    node_test = world.create_init_node()
    node_test.run()

    hivemind = Hivemind(
        database_name='hivemind_pyt',
        database_user='dev',
        database_password='devdevdev',
    )
    hivemind.run(sync_with=node_test)
    # time.sleep(5)
    message = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"hive.db_head_state","params":{}})
    print()

    # run(sync_with=node_test, use_existing_db=True, run_sync=True, run_server=True)