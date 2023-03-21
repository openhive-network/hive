import json
import time
from typing import Final

import pytest

import test_tools as tt

ALTERNATE_CHAIN_JSON_FILENAME: Final[str] = "alternate-chain-spec.json"


def test_simply_hardfork_schedule():
    """
    Hardfork schedule depends on time. Therefore, possible is situation the hard works will be applying in other
    blocks in different runs. Slight delays are caused by a.o. using in this test faketime to increase speed of test,
    decrease of efficiency on CI or inaccuracy of synchronization 'genesis_time' with application of hf_00.
    To make compensation of blocks delay is used blocks_delay_margin.
    """
    blocks_delay_margin = 3

    init_supply = 1000000
    hbd_init_supply = 2000000
    hardfork_schedule = [
        {"hardfork": 2, "block_num": 0},
        {"hardfork": 20, "block_num": 20},
        {"hardfork": 27, "block_num": 380}
    ]
    witnesses_required_for_hf06_and_later = [f'witness-{witness_num}' for witness_num in range(16)]
    alternate_chain_spec = build_alternate_chain_spec(hardfork_schedule, witnesses_required_for_hf06_and_later,
                                                      init_supply, hbd_init_supply)

    init_node = tt.InitNode()
    for witness in witnesses_required_for_hf06_and_later:
        init_node.config.witness.append(witness)
    write_to_json(alternate_chain_spec)
    init_node.run(time_offset="+0 x20", arguments=['--alternate-chain-spec',
                                                   str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)])

    # verify are hardforks 0-2 were applied correctly
    assert is_hardfork_applied(init_node, hf_number=2)

    # verify are hardforks 3-5 were applied correctly
    init_node.wait_for_block_with_number(20 + blocks_delay_margin)
    assert is_hardfork_applied(init_node, hf_number=5)

    # verify are hardforks 6-20 were applied correctly
    for i in range(15):
        # Hardforks are applying in every witness schedules (every 21 blocks).
        init_node.wait_for_block_with_number(42 + blocks_delay_margin + i * 21)
        assert is_hardfork_applied(init_node, hf_number=6 + i)

    # verify are hardforks 21-28 were applied correctly
    for i in range(8):
        # Hardforks are applying in every witness schedules (every 21 blocks).
        init_node.wait_for_block_with_number(380 + blocks_delay_margin + i * 21)
        assert is_hardfork_applied(init_node, hf_number=21 + i)


@pytest.mark.parametrize(
    'hardfork_schedule',
    [
        pytest.param(
            [
                {"hardfork": 2, "block_num": 0},
                {"hardfork": 27, "block_num": 380},
                {"hardfork": 20, "block_num": 20},
            ],
            id='wrong order'
        ),

        pytest.param(
            [
                {"hardfork": 20, "block_num": 0},
                {"hardfork": 2, "block_num": 1},
                {"hardfork": 27, "block_num": 25}
            ],
            id='wrong order of hardforks'
        ),

        pytest.param(
            [
                {"hardfork": 2, "block_num": 1},
                {"hardfork": 20, "block_num": 0},
                {"hardfork": 27, "block_num": 380}
            ],
            id='blocks are not constantly growing'
        ),

        pytest.param(
            [
            ],
            id='empty hardfork schedule'
        ),

        pytest.param(
            [
                {"hardfork": 50, "block_num": 0},
            ],
            id='nonexistent hardfork'
        ),

        pytest.param(
            [
                {"hardfork": 0, "block_num": 0},
            ],
            id='hf_00 is forbidden in hardfork schedule'
        ),
    ],
)
def test_incorrect_hardfork_schedules(hardfork_schedule):
    alternate_chain_spec = build_alternate_chain_spec(hardfork_schedule)

    init_node = tt.InitNode()
    write_to_json(alternate_chain_spec)
    with pytest.raises(TimeoutError):
        init_node.run(
            arguments=['--alternate-chain-spec',
                       str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)])


@pytest.mark.parametrize(
    'keys_to_drop', [
        ['hardfork_schedule'],
        ['genesis_time'],
    ]
)
def test_alternate_chain_spec_necessary_keys(keys_to_drop):
    hardfork_schedule = [
        {"hardfork": 2, "block_num": 0},
    ]

    alternate_chain_spec = build_alternate_chain_spec(hardfork_schedule)
    drop_keys_from(alternate_chain_spec, *keys_to_drop)

    init_node = tt.InitNode()
    write_to_json(alternate_chain_spec)
    with pytest.raises(TimeoutError):
        init_node.run(
            arguments=['--alternate-chain-spec',
                       str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)])


@pytest.mark.parametrize(
    'keys_to_drop', [
        ['init_witnesses', 'hbd_init_supply', 'init_supply'],
        ['hbd_init_supply', 'init_supply'],
        ['init_witnesses', 'init_supply'],
        ['init_witnesses', 'hbd_init_supply'],
        ['init_witnesses'],
        ['init_supply'],
        ['hbd_init_supply']
    ]
)
def test_alternate_chain_spec_optional_keys(keys_to_drop):
    init_supply = 1000000
    hbd_init_supply = 2000000
    hardfork_schedule = [
        {"hardfork": 2, "block_num": 0},
    ]
    witnesses_required_for_hf06_and_later = [f'witness-{witness_num}' for witness_num in range(16)]
    alternate_chain_spec = build_alternate_chain_spec(hardfork_schedule, witnesses_required_for_hf06_and_later,
                                                      init_supply, hbd_init_supply)
    drop_keys_from(alternate_chain_spec, *keys_to_drop)

    init_node = tt.InitNode()
    write_to_json(alternate_chain_spec)
    init_node.run(
        arguments=['--alternate-chain-spec', str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)])


@pytest.mark.parametrize(
    'witnesses', [
        [1324, 123456],
        ['1234', '123456'],
        ['ABC', '.abcde'],
        ['a', 'ab'],
    ]
)
def test_invalid_witness_names(witnesses):
    hardfork_schedule = [
        {"hardfork": 2, "block_num": 0},
    ]
    alternate_chain_spec = build_alternate_chain_spec(hardfork_schedule, witnesses)
    init_node = tt.InitNode()
    write_to_json(alternate_chain_spec)
    with pytest.raises(TimeoutError):
        init_node.run(
            arguments=['--alternate-chain-spec',
                       str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)])


def drop_keys_from(container: dict, *keys_to_drop) -> None:
    for key in keys_to_drop:
        del container[key]


def build_alternate_chain_spec(hardfork_schedule: list, init_witnesses: list = None, init_supply: int = None,
                               hbd_init_supply: int = None) -> dict:
    return {
        "genesis_time": int(time.time()),
        "hardfork_schedule": hardfork_schedule,
        **({"init_witnesses": init_witnesses} if init_witnesses else {}),
        **({"init_supply": init_supply} if init_supply else {}),
        **({"hbd_init_supply": hbd_init_supply} if hbd_init_supply else {}),
    }


def write_to_json(alternate_chain_spec_content: dict) -> None:
    directory = tt.context.get_current_directory()
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / ALTERNATE_CHAIN_JSON_FILENAME, 'w') as json_file:
        json.dump(alternate_chain_spec_content, json_file)


def is_hardfork_applied(node, hf_number: int) -> bool:
    hf_version = f"{'0' if hf_number < 24 else '1'}.{hf_number}.0"
    return node.api.wallet_bridge.get_hardfork_version() == hf_version
