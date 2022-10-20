from .local_tools import prepare_wallets, legacy_operation_passed, hf26_operation_failed

def test_before_hf26(node_hf25):
    wallet_legacy, wallet_hf26 = prepare_wallets(node_hf25)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)
