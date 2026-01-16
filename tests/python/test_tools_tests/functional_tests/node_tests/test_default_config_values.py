# This test forces that create_default_config will be always up to date.
from __future__ import annotations

import pytest
import test_tools as tt
from test_tools.__private.process.node_config import NodeConfig


@pytest.fixture
def generated_config() -> NodeConfig:
    node = tt.RawNode()
    node.set_cleanup_policy(tt.constants.CleanupPolicy.REMOVE_EVERYTHING)
    node.dump_config()

    return node.config


def test_default_config_values(generated_config: NodeConfig) -> None:
    default_config = NodeConfig.default(skip_address=False)
    if default_config != generated_config:
        tt.logger.info("Found differences:")
        differences = default_config.get_differences_between(generated_config)

        for key, (default_value, generated_value) in differences.items():
            tt.logger.info(
                f"""{key}

In default_config: {default_value} {type(default_value)}
Generated:         {generated_value} {type(generated_value)}"""
            )

        raise AssertionError("Modify config returned from create_default_config to match default generated config")
