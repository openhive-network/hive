import test_tools as tt


def test_claim_account_creation(node, wallet):
    node.wait_number_of_blocks(18)
    wallet.api.claim_account_creation('initminer', tt.Asset.Test(0))
