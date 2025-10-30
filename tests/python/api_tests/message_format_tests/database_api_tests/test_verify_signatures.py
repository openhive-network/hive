from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from wax import calculate_sig_digest
from wax._private.result_tools import expose_result_as_python_string


# verify_signatures tests cannot be performed on 5 million and live_mainnet. Generate_sig_digest function requires
# private key of real account which signed transaction. This key is kept in secret and the only person that know it
# is its owner.
@run_for("testnet")
def test_verify_signatures_in_testnet(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    transaction = wallet.api.create_account("initminer", "alice", "{}")
    node_config = node.api.database.get_config()
    sig_digest = calculate_sig_digest(
        transaction.json(),
        str(node_config.HIVE_CHAIN_ID),
    )

    node.api.database.verify_signatures(
        hash=expose_result_as_python_string(sig_digest),
        signatures=transaction["signatures"],
        required_owner=[],
        required_active=["initminer"],
        required_posting=[],
        required_other=[],
    )
