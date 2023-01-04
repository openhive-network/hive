from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture
def replayed_node():
    api_node = tt.ApiNode()
    api_node.run(replay_from=Path(__file__).parent / "block_log" / "block_log", wait_for_live=False)
    return api_node


@pytest.fixture
def wallet_with_pattern_name(replayed_node, request):
    if "cli_wallet_method" in request.fixturenames:
        pattern_name = request.getfixturevalue("cli_wallet_method")
    else:
        method_name: str = request.function.__name__
        assert method_name.startswith("test_")
        pattern_name = method_name[len("test_") :]

    wallet = tt.Wallet(
        attach_to=replayed_node,
        additional_arguments=[f"--store-transaction={pattern_name}", f"--transaction-serialization={request.param}"],
    )

    for account_name in ["alice", "initminer", "dan", "bob"]:
        wallet.api.import_key(tt.Account(account_name).private_key)

    return wallet, pattern_name
