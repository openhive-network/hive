from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools.functional.python.operation.custom_and_custom_json import Custom

if TYPE_CHECKING:
    import test_tools as tt
    from hive_local_tools.functional.python.operation import Account


@pytest.mark.parametrize(
    ("sign_transaction_authors", "required_auths"),
    [
        (["alice"], ["alice"]),
        (["alice", "bob"], ["alice", "bob"]),
    ],
)
@pytest.mark.testnet()
def test_custom(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_alice_and_bob: tuple[Account, Account],
    sign_transaction_authors: list[str],
    required_auths: list[str],
) -> None:
    # test cases from https://gitlab.syncad.com/hive/hive/-/issues/644
    alice, bob = create_alice_and_bob
    custom = Custom(prepared_node, wallet)
    trx = custom.generate_transaction(required_auths, 1, "cdc7be78dc7")

    for signature_number in range(len(sign_transaction_authors)):
        broadcast = signature_number == len(sign_transaction_authors) - 1
        wallet.api.use_authority("active", sign_transaction_authors[signature_number])
        broadcasted = custom.sign_transaction(trx, broadcast=broadcast)

    alice.check_if_current_rc_mana_was_reduced(broadcasted)
