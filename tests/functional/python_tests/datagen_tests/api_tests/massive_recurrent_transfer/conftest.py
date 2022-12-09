from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture
def replayed_node():
    block_log_directory = Path(__file__).parent / 'block_log'
    block_log_path = block_log_directory / "block_log"
    timestamp_path = block_log_directory / "timestamp"

    with open(timestamp_path, encoding='utf-8') as file:
        timestamp = tt.Time.parse(file.read())

    timestamp -= tt.Time.seconds(5)

    init_node = tt.InitNode()

    init_node.run(
        time_offset=tt.Time.serialize(timestamp, format_=tt.Time.TIME_OFFSET_FORMAT),
        replay_from=block_log_path,
    )

    return init_node
