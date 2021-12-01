from pathlib import Path
import re
import subprocess
import time

from test_tools import communication, logger, World, Wallet, hivemind
# database = "postgresql://dev:devdevdev@localhost:5432/hivemind_pyt"
def test_hivemind_start_sync(world: World):
    node_test = world.create_init_node()
    node_test.run()

    hivemind1 = hivemind.Hivemind(
        host='localhost',
        port='5432',
        database_name='hivemind_pyt',
        user='dev',
        password='devdevdev',
        node=node_test
    )

    hivemind1.environment_setup()
    hivemind1.run_sync(node_test)  # sync
    hivemind1.run_server() #server
    time.sleep(10)
    message = communication.request(url='http://localhost:8080', message={"jsonrpc":"2.0","id":0,"method":"hive.db_head_state","params":{}})
    time.sleep(100)

    # communication.request()
    # accounts_number = hivemind1.how_many_accounts()
