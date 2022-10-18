from pyrsistent import v


from .local_tools import prepare_wallets, legacy_operation_passed, hf26_operation_passed

def test_after_hf26(node_hf26):
    wallet_legacy, wallet_hf26 = prepare_wallets(node_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_passed(wallet_hf26)
