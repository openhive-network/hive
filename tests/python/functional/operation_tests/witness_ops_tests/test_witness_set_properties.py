from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import WitnessAccount

@pytest.mark.parametrize(
    "update_set_properties_args",
    [
        {"new_url": "http://new-url.html"},
        # {"new_maximum_block_size": 131070},
        # {"new_hbd_interest_rate": 1111},
        # {"new_block_signing_key": tt.Account("test-name").public_key},
        # {"new_account_creation_fee": tt.Asset.Test(31)},
        # {
        #     "new_url": "http://new-url.html",
        #     "new_maximum_block_size": 131070,
        #     "new_hbd_interest_rate": 1111,
        #     "new_block_signing_key": tt.Account("test-name").public_key,
        #     "new_account_creation_fee": tt.Asset.Test(31),
        # },
    ],
)
@pytest.mark.testnet()
def test_witness_set_properties(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount, update_set_properties_args
) -> None:
    from hive_local_tools.constants import TRANSACTION_TEMPLATE
    # test case 1.1 - 1.9 from https://gitlab.syncad.com/hive/hive/-/issues/633
    alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.check_if_account_has_witness_role(expected_witness_role=True)
    # alice.rc_manabar.update()
    witnesses = prepared_node.api.database.list_witnesses(start="", limit=100, order="by_name").witnesses
    from hive_local_tools.constants import get_transaction_model
    q = get_transaction_model()
    op = ['witness_set_properties_operation', {'owner': 'alice', }]
    q.operations.append()
    from schemas.operations.witness_block_approve_operation import WitnessBlockApproveOperation


    # transaction = {
    #     "ref_block_num": 0,
    #     "ref_block_prefix": 0,
    #     "expiration": "1970-01-01T00:00:00",
    #     "operations": [
    #         {
    #           "type": "witness_set_properties_operation",
    #           "value": {
    #             "owner": "alice",
    #             "props": [
    #               [
    #                 "hbd_exchange_rate",
    #                 "b6010000000000000353424400000000e80300000000000003535445454d0000"
    #               ],
    #               [
    #                 "key",
    #                 "0391521b22560cbf647f330c3d3bd80a2e36ac720fe5ef383b1edbcc016ba57f03"
    #               ]
    #             ],
    #             "extensions": []
    #           }
    #         }
    #     ],
    #     "extensions": [],
    #     "signatures": [],
    #     "transaction_id": "0000000000000000000000000000000000000000",
    #     "block_num": 0,
    #     "transaction_num": 0,
    # }
    # q = wallet.api.sign_transaction(transaction)
    print()
    # trx = alice.
    # alice.assert_if_rc_current_mana_was_reduced(trx)
    # alice.assert_if_feed_publish_operation_was_generated(trx)
