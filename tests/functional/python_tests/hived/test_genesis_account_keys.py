import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_genesis_account_keys(node):
    wallet = tt.Wallet(attach_to=node)
    for account in wallet.list_accounts():
        wallet.api.get_account(account)
