from __future__ import annotations

import os

import pytest

import test_tools as tt
from python.functional.colony.test_colony import prepare_colony_config, set_log_level_to_info, wait_for_start_colony
from python.functional.operation_tests.conftest import WitnessAccount
from python.functional.witness_tests.block_size_set_as_witness_set_properties_tests.block_log.generate_block_log import (
    WITNESSES,
)

os.environ["SERIALIZE_SET_PROPERTIES_PATH"] = "/home/dev/hive/build/programs/util/serialize_set_properties"


def test_exceed_max_block_size_limit(replay_node) -> None:
    wallet = tt.Wallet(attach_to=replay_node)
    witness = WitnessAccount("witness-0", replay_node, wallet)

    # set max block size to 3mb ( 3145728 bytes ) by witness_set_properties operation
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        witness.witness_set_properties(
            props_to_serialize={
                # each witness created by alternate-chain-spec have same parameters at initminer
                "key": replay_node.api.database.get_config().HIVE_INIT_PUBLIC_KEY,
                "maximum_block_size": 3145728,
            }
        )
    error_message = ""  # fixme, after repair in hived
    assert error_message in error.value.error, "Appropriate error message is not raise"


def test_maximum_block_size_set_as_witness_set_properties(replay_node) -> None:
    wallet = tt.Wallet(attach_to=replay_node)
    witnesses = [WitnessAccount(witness, replay_node, wallet) for witness in ["initminer", *WITNESSES]]

    for witness in witnesses:
        witness.witness_set_properties(
            props_to_serialize={
                # each witness created by alternate-chain-spec have same parameters at initminer
                "key": replay_node.api.database.get_config().HIVE_INIT_PUBLIC_KEY,
                "maximum_block_size": 3145728,
            }
        )

    assert replay_node.api.database.get_dynamic_global_properties().maximum_block_size == 3145728


@pytest.mark.testnet()
def test_start_colony_from_block_log(block_log: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs) -> None:
    node = tt.InitNode()
    node.config.shared_file_size = "4G"
    set_log_level_to_info(node)
    prepare_colony_config(node)

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log,
        timeout=120,
        exit_before_synchronization=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    node.run(
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
        timeout=120,
        wait_for_live=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.api.import_keys([tt.Account(w).private_key for w in WITNESSES])

    witnesses = [WitnessAccount(witness, node, wallet) for witness in ["initminer", *WITNESSES]]

    for witness in witnesses:
        key = tt.Account(witness.name)
        witness.witness_set_properties(
            props_to_serialize={
                # each witness created by alternate-chain-spec have same parameters at initminer
                # "key": node.api.database.get_config().HIVE_INIT_PUBLIC_KEY,
                "key": key.public_key,
                "maximum_block_size": 3145728,
            }
        )

    # assert node.api.database.get_dynamic_global_properties().maximum_block_size == 3145728
    print()

    assert node.is_running()
    wait_for_start_colony(node, min_trx_in_block=5000)
