import pytest

import test_tools as tt

from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD

@pytest.fixture()
def prepare_environment():
    init_node = tt.InitNode()
    init_node.run(time_offset="+0h x5")

    wallet_alice_agent = tt.Wallet(attach_to=init_node)
    wallet_alice_agent.create_account("alice.agent", vests=100)

    wallet_alice = tt.Wallet(attach_to=init_node)
    wallet_alice.create_account("alice", vests=100)
    wallet_alice.api.change_recovery_account("alice", "alice.agent")

    wallet_thief_agent = tt.Wallet(attach_to=init_node)
    wallet_thief_agent.create_account("thief.agent", vests=100)

    wallet_thief = tt.Wallet(attach_to=init_node)
    wallet_thief.create_account("thief", vests=100)
    wallet_thief.api.import_key(tt.Account("alice").private_key) # thief steal alice-key - he adds her key to his wallet
    wallet_thief.api.change_recovery_account("thief", "thief.agent")

    init_node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    return init_node, wallet_alice, wallet_thief, wallet_alice_agent, wallet_thief_agent
