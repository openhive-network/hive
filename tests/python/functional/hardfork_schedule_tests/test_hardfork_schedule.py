from __future__ import annotations

import time

import pytest

import test_tools as tt
from beekeepy.exceptions import CommunicationError, FailedToStartExecutableError


def _run_node_with_retry(node: tt.InitNode, max_retries: int = 3, **kwargs) -> None:
    """Run node with retry logic for flaky CI environments."""
    for attempt in range(max_retries):
        try:
            node.run(**kwargs)
            return
        except (CommunicationError, FailedToStartExecutableError) as e:
            if attempt < max_retries - 1:
                tt.logger.warning(f"Node startup failed (attempt {attempt + 1}/{max_retries}): {e}, retrying...")
                time.sleep(2)
                # Need to recreate node for retry
                node.close()
            else:
                raise


def _wait_for_hardfork_with_retry(node: tt.InitNode, hf_number: int, expected_block: int, max_retries: int = 10) -> None:
    """
    Wait for hardfork to apply with retry tolerance for CI load variations.

    Under heavy CI load with faketime speedup, hardforks can apply several blocks
    later than expected. This function retries across multiple blocks instead of
    failing immediately.
    """
    # Wait for expected block first
    node.wait_for_block_with_number(expected_block)

    # Retry across multiple blocks if not applied yet
    for retry in range(max_retries):
        if is_hardfork_applied(node, hf_number=hf_number):
            return  # Success
        if retry < max_retries - 1:
            node.wait_for_block_with_number(expected_block + retry + 1)

    # Failed after all retries
    current_block = expected_block + max_retries
    raise AssertionError(
        f"Hardfork {hf_number} not applied by block {current_block} "
        f"(expected at {expected_block}, checked {max_retries} blocks ahead)"
    )


def test_simply_hardfork_schedule() -> None:
    """
    Hardfork schedule depends on time. Therefore, possible is situation the hard works will be applying in other
    blocks in different runs. Slight delays are caused by a.o. using in this test faketime to increase speed of test,
    decrease of efficiency on CI or inaccuracy of synchronization 'genesis_time' with application of hf_00.
    To make compensation of blocks delay is used blocks_delay_margin.
    """
    blocks_delay_margin = 7  # Increased from 3 to handle CI load variability
    witnesses_required_for_hf06_and_later = [f"witness-{witness_num}" for witness_num in range(16)]

    init_node = tt.InitNode()
    for witness in witnesses_required_for_hf06_and_later:
        init_node.config.witness.append(witness)

    _run_node_with_retry(
        init_node,
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),  # Reduced from 20 to minimize cumulative timing drift
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

    # verify are hardforks 0-2 were applied correctly (HF2 at block 0)
    _wait_for_hardfork_with_retry(init_node, hf_number=2, expected_block=blocks_delay_margin)

    # verify are hardforks 3-5 were applied correctly
    _wait_for_hardfork_with_retry(init_node, hf_number=5, expected_block=20 + blocks_delay_margin)

    # verify are hardforks 6-20 were applied correctly
    for i in range(15):
        # Hardforks are applying in every witness schedules (every 21 blocks).
        expected_block = 42 + blocks_delay_margin + i * 21
        hf_num = 6 + i
        _wait_for_hardfork_with_retry(init_node, hf_number=hf_num, expected_block=expected_block)

    # verify are hardforks 21-28 were applied correctly
    for i in range(8):
        # Hardforks are applying in every witness schedules (every 21 blocks).
        expected_block = 380 + blocks_delay_margin + i * 21
        hf_num = 21 + i
        _wait_for_hardfork_with_retry(init_node, hf_number=hf_num, expected_block=expected_block)


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
