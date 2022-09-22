import test_tools as tt

from .local_tools import generate_sig_digest


def test_verify_signatures(node, wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}')
    sig_digest = generate_sig_digest(transaction, tt.Account('initminer').private_key)
    node.api.database.verify_signatures(hash=sig_digest, signatures=transaction['signatures'],
                                        required_owner=[], required_active=['initminer'],
                                        required_posting=[], required_other=[])
