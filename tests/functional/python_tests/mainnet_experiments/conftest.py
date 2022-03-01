import pytest

from test_tools import BlockLog


@pytest.fixture
def node(world):
    api_node = world.create_api_node()
    api_node.config.shared_file_size = '8G'
    api_node.run(
        replay_from=BlockLog('/usr/local/hive/blockchain/block_log.5M', include_index=False),
        stop_at_block=5_000_000,
        wait_for_live=False,
    )

    yield api_node
