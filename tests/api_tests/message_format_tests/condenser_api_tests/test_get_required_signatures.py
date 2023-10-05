import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_get_required_signatures_in_testnet(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    keys = node.api.condenser.get_required_signatures(transaction, [tt.Account("initminer").public_key])
    assert len(keys) != 0


@run_for("mainnet_5m", "live_mainnet")
def test_get_required_signatures_in_mainnet(node):
    block = node.api.condenser.get_block(3652254)
    account = node.api.wallet_bridge.get_accounts(["steem"])[0]
    transaction = block["transactions"][3]
    active_key = account["active"]["key_auths"][0][0]
    keys = node.api.condenser.get_required_signatures(transaction, [active_key])
    assert len(keys) != 0
