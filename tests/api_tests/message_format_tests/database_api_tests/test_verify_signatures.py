import test_tools as tt

from .local_tools import generate_sig_digest
from ..local_tools import run_for


# verify_signatures tests cannot be performed on 5 million and 64 million mainnet. Generate_sig_digest function requires
# private key of real account which signed transaction. This key is kept in secret and the only person that know it
# is its owner.
@run_for('testnet')
def test_verify_signatures_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node, additional_arguments=['--transaction-serialization=hf26'])
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    sig_digest = generate_sig_digest(transaction, tt.Account('initminer').private_key)
    prepared_node.api.database.verify_signatures(hash=sig_digest, signatures=transaction['signatures'],
                                                 required_owner=[], required_active=['initminer'],
                                                 required_posting=[], required_other=[])
