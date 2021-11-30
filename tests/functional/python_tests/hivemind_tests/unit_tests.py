from pathlib import Path
import re
import subprocess
import time

from test_tools import communication, logger, World, Wallet, hivemind

database = "postgresql://dev:devdevdev@localhost:5432/hivemind_pyt"

def test_hivemind_start_sync(world:World):
    node_test = world.create_init_node()
    node_test.run()
    hivemind1 = hivemind.Sync(database_adress=database, node=node_test)
    environment = hivemind1.environment_setup()
    hivemind1.run(node_test)  # sync + server
    time.sleep(100)
    # communication.request()
    # accounts_number = hivemind1.how_many_accounts()
