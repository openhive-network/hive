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
        sync_with=node_test
    )
    hivemind.detabase_prepare()
    hivemind.run()
    time.sleep(10)
    message = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"hive.db_head_state","params":{}})
    print()

