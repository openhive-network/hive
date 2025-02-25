from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import filters_enum_virtual_ops
from hive_local_tools.functional import connect_nodes
from schemas.fields.basic import AccountName
from schemas.operations.representations.util import convert_to_representation
from schemas.operations.virtual.producer_reward_operation import ProducerRewardOperation


@run_for("testnet", enable_plugins=["account_history_api"])
def test_transaction_timestamp_consistency_before_and_after_irreversibility(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    trx = wallet.api.create_account("initminer", "alice", "{}")

    reversible_ah_trx = node.api.account_history_api.get_ops_in_block(block_num=trx.block_num, include_reversible=True)

    node.wait_for_block_with_number(30)  # wait for activate obi
    assert node.api.database.get_dynamic_global_properties().last_irreversible_block_num >= 2

    irreversible_ah_trx = node.api.account_history_api.get_ops_in_block(
        block_num=trx.block_num, include_reversible=False
    )

    assert reversible_ah_trx.ops[0].timestamp == irreversible_ah_trx.ops[0].timestamp, "Timestamps isn't the same"


def test_account_history_data_consistency_on_replayed_and_full_pruned_node(
    block_log_empty_30_mono: tt.BlockLog,
) -> None:
    node = tt.InitNode()
    node.config.enable_stale_production = True
    node.config.required_participation = 0

    # fixme: after repair use `stop_at_block=50`
    node.run(replay_from=block_log_empty_30_mono)

    api_node = tt.ApiNode()
    api_node.config.plugin.append("account_history_api")
    api_node.config.block_log_split = 0
    connect_nodes(node, api_node)
    api_node.run()

    check_at_block: int = 50
    api_node.wait_for_block_with_number(check_at_block)  # wait for api node catch up producer node

    vops = api_node.api.account_history.enum_virtual_ops(
        block_range_begin=0,
        block_range_end=check_at_block + 1,  # last block number exclusive
        include_reversible=False,
        filter_=filters_enum_virtual_ops["producer_reward_operation"],
    ).ops

    assert api_node.get_last_block_number() == node.get_last_block_number()
    assert len(vops) == node.get_last_block_number()
    assert vops[check_at_block - 5].op == convert_to_representation(
        ProducerRewardOperation(producer=AccountName("initminer"), vesting_shares=tt.Asset.Vest(0.140274))
    )
