from hive_local_tools.functional.python.hf26 import hf26_operation_passed, legacy_operation_passed, prepare_wallets


def test_after_hf26(node_hf26):
    wallet_legacy, wallet_hf26 = prepare_wallets(node_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_passed(wallet_hf26)
