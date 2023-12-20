from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation
from schemas.fields.compound import Authority
from schemas.operations.account_create_operation import AccountCreateOperation


@pytest.fixture()
def prepared_network():
    network = tt.Network()

    tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)

    network.run()
    # Force hf26 serialization to send transaction directly without serialization conversion
    api_node_wallet = tt.Wallet(attach_to=api_node, additional_arguments=["--transaction-serialization=hf26"])
    api_node_wallet.api.set_transaction_expiration(3600 - 1)

    with api_node_wallet.in_single_transaction():
        auth_account_names = [f"auth-account-{num}" for num in range(40)]
        for name in auth_account_names:
            create_account_with_maximum_number_of_keys(api_node_wallet, name, key_auths=40)

    for name in auth_account_names:
        for auth_type in ["owner", "active", "posting"]:
            api_node_wallet.api.import_keys(
                [tt.Account(name, f"{name}-{auth_type}-{num}").private_key for num in range(40)]
            )

    create_account_with_maximum_number_of_keys(
        api_node_wallet, "alice", account_auths=[[name, 1] for name in auth_account_names], key_auths=0
    )

    api_node_wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "memo")

    return api_node, api_node_wallet


def create_account_with_maximum_number_of_keys(
    wallet: tt.Wallet,
    account_name: str,
    account_auths: list[list[str, int]] | None = None,
    key_auths: list[list[str, int]] | None = None,
) -> None:
    create_transaction_with_any_operation(
        wallet,
        AccountCreateOperation(
            fee=tt.Asset.Test(0),
            creator="initminer",
            new_account_name=account_name,
            owner=Authority(
                weight_threshold=40,
                account_auths=[] if account_auths is None else account_auths,
                key_auths=(
                    []
                    if key_auths < 0
                    else [
                        [tt.Account(account_name, f"{account_name}-owner-{num}").public_key, 1]
                        for num in range(key_auths)
                    ]
                ),
            ),
            active=Authority(
                weight_threshold=40,
                account_auths=[] if account_auths is None else account_auths,
                key_auths=(
                    []
                    if key_auths < 0
                    else [
                        [tt.Account(account_name, f"{account_name}-active-{num}").public_key, 1]
                        for num in range(key_auths)
                    ]
                ),
            ),
            posting=Authority(
                weight_threshold=40,
                account_auths=[] if account_auths is None else account_auths,
                key_auths=(
                    []
                    if key_auths < 0
                    else [
                        [tt.Account(account_name, f"{account_name}-posting-{num}").public_key, 1]
                        for num in range(key_auths)
                    ]
                ),
            ),
            memo_key=tt.PublicKey(account_name, secret=f"memo-{account_name}"),
            json_metadata="{}",
        ),
    )
