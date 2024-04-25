from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy._executable.beekeeper_config import (
    BeekeeperConfig,
    http_webserver_default,
)
from beekeepy._executable.defaults import BeekeeperDefaults

if TYPE_CHECKING:
    from beekeepy._executable.beekeeper_executable import BeekeeperExecutable


def check_default_values_from_config(default_config: BeekeeperConfig) -> None:
    assert default_config.wallet_dir.resolve() == BeekeeperDefaults.DEFAULT_WALLET_DIR.resolve()
    assert default_config.unlock_timeout == BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    assert default_config.unlock_interval == BeekeeperDefaults.DEFAULT_UNLOCK_INTERVAL
    assert default_config.log_json_rpc == BeekeeperDefaults.DEFAULT_LOG_JSON_RPC
    assert default_config.webserver_http_endpoint == http_webserver_default()
    assert default_config.webserver_thread_pool_size == BeekeeperDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE
    assert default_config.notifications_endpoint == BeekeeperDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT
    assert default_config.backtrace == BeekeeperDefaults.DEFAULT_BACKTRACE
    assert default_config.export_keys_wallet == BeekeeperDefaults.DEFAULT_EXPORT_KEYS_WALLET


def test_default_values(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check default values of Beekeeper."""
    # ARRANGE & ACT
    default_config = beekeeper_exe.generate_default_config()

    # ASSERT
    check_default_values_from_config(default_config)
