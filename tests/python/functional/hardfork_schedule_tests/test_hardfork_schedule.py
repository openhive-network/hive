from __future__ import annotations

import pytest

import test_tools as tt
from beekeepy.exceptions import FailedToStartExecutableError


def test_simply_hardfork_schedule() -> None:
    """
    Hardfork schedule depends on time. Therefore, possible is situation the hard works will be applying in other
    blocks in different runs. Slight delays are caused by a.o. using in this test faketime to increase speed of test,
    decrease of efficiency on CI or inaccuracy of synchronization 'genesis_time' with application of hf_00.
    To make compensation of blocks delay is used blocks_delay_margin.
    """
    blocks_delay_margin = 3
    witnesses_required_for_hf06_and_later = [f"witness-{witness_num}" for witness_num in range(16)]

    init_node = tt.InitNode()
    for witness in witnesses_required_for_hf06_and_later:
        init_node.config.witness.append(witness)

    init_node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=20),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=2, block_num=0),
                tt.HardforkSchedule(hardfork=20, block_num=20),
                tt.HardforkSchedule(hardfork=27, block_num=380),
            ],
            init_supply=1000000,
            hbd_init_supply=2000000,
            init_witnesses=witnesses_required_for_hf06_and_later,
        ),
    )

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
    "hardfork_schedule",
    [
        pytest.param(
            [
                tt.HardforkSchedule(hardfork=2, block_num=0),
                tt.HardforkSchedule(hardfork=27, block_num=380),
                tt.HardforkSchedule(hardfork=20, block_num=20),
            ],
            id="wrong order",
        ),
        pytest.param(
            [
                tt.HardforkSchedule(hardfork=20, block_num=0),
                tt.HardforkSchedule(hardfork=2, block_num=1),
                tt.HardforkSchedule(hardfork=27, block_num=25),
            ],
            id="wrong order of hardforks",
        ),
        pytest.param(
            [
                tt.HardforkSchedule(hardfork=2, block_num=1),
                tt.HardforkSchedule(hardfork=20, block_num=0),
                tt.HardforkSchedule(hardfork=27, block_num=380),
            ],
            id="blocks are not constantly growing",
        ),
        pytest.param([], id="empty hardfork schedule"),
        pytest.param(
            [
                tt.HardforkSchedule(hardfork=50, block_num=0),
            ],
            id="nonexistent hardfork",
        ),
        pytest.param(
            [
                tt.HardforkSchedule(hardfork=0, block_num=0),
            ],
            id="hf_00 is forbidden in hardfork schedule",
        ),
    ],
)
def test_incorrect_hardfork_schedules(hardfork_schedule: list[tt.HardforkSchedule]) -> None:
    init_node = tt.InitNode()
    with pytest.raises(FailedToStartExecutableError):
        init_node.run(
            alternate_chain_specs=tt.AlternateChainSpecs(
                genesis_time=int(tt.Time.now(serialize=False).timestamp()),
                hardfork_schedule=hardfork_schedule,
            )
        )


@pytest.mark.parametrize(
    "keys_to_drop",
    [
        ["hardfork_schedule"],
        ["genesis_time"],
    ],
)
def test_alternate_chain_spec_necessary_keys(keys_to_drop: list[str]) -> None:
    alternate_chain_spec = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=2, block_num=0)],
    )

    drop_keys_from(alternate_chain_spec, *keys_to_drop)

    init_node = tt.InitNode()
    with pytest.raises(FailedToStartExecutableError):
        init_node.run(alternate_chain_specs=alternate_chain_spec)


@pytest.mark.parametrize(
    "keys_to_drop",
    [
        ["init_witnesses", "hbd_init_supply", "init_supply"],
        ["hbd_init_supply", "init_supply"],
        ["init_witnesses", "init_supply"],
        ["init_witnesses", "hbd_init_supply"],
        ["init_witnesses"],
        ["init_supply"],
        ["hbd_init_supply"],
    ],
)
def test_alternate_chain_spec_optional_keys(keys_to_drop: list[str]) -> None:
    alternate_chain_spec = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=2, block_num=0)],
        init_witnesses=[f"witness-{witness_num}" for witness_num in range(16)],
        init_supply=1000000,
        hbd_init_supply=2000000,
    )
    drop_keys_from(alternate_chain_spec, *keys_to_drop)

    init_node = tt.InitNode()
    init_node.run(alternate_chain_specs=alternate_chain_spec)
    assert init_node.is_running()


@pytest.mark.parametrize(
    "witnesses",
    [
        [1324, 123456],
        ["1234", "123456"],
        ["ABC", ".abcde"],
        ["a", "ab"],
    ],
)
def test_invalid_witness_names(witnesses: list[int | str]) -> None:
    init_node = tt.InitNode()
    with pytest.raises(FailedToStartExecutableError):
        init_node.run(
            alternate_chain_specs=tt.AlternateChainSpecs(
                genesis_time=int(tt.Time.now(serialize=False).timestamp()),
                hardfork_schedule=[tt.HardforkSchedule(hardfork=2, block_num=0)],
                init_witnesses=witnesses,
            )
        )


def drop_keys_from(container: tt.AlternateChainSpecs, *keys_to_drop) -> None:
    for key in keys_to_drop:
        setattr(container, key, None)


def is_hardfork_applied(node, hf_number: int) -> bool:
    hf_version = f"{'0' if hf_number < 24 else '1'}.{hf_number}.0"
    return node.api.wallet_bridge.get_hardfork_version() == hf_version
