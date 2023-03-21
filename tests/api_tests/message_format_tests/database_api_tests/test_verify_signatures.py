import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format.database_api import generate_sig_digest


# verify_signatures tests cannot be performed on 5 million and live_mainnet. Generate_sig_digest function requires
# private key of real account which signed transaction. This key is kept in secret and the only person that know it
# is its owner.
@run_for('testnet')
def test_verify_signatures_in_testnet(node):
    wallet = tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    sig_digest = generate_sig_digest(transaction, tt.Account('initminer').keys.private)
    node.api.database.verify_signatures(hash=sig_digest, signatures=transaction['signatures'], required_owner=[],
                                        required_active=['initminer'], required_posting=[], required_other=[])
