from __future__ import annotations

from clive.__private.core.beekeeper import Beekeeper
from clive.__private.core.beekeeper.config import BeekeeperConfig, webserver_default
from clive.__private.core.beekeeper.defaults import BeekeeperDefaults


def check_default_values_from_config(default_config: BeekeeperConfig) -> None:
    assert default_config.wallet_dir.resolve() == BeekeeperDefaults.DEFAULT_WALLET_DIR.resolve()
    assert default_config.unlock_timeout == BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    assert default_config.unlock_interval == BeekeeperDefaults.DEFAULT_UNLOCK_INTERVAL
    assert default_config.log_json_rpc == BeekeeperDefaults.DEFAULT_LOG_JSON_RPC
    assert default_config.webserver_http_endpoint == webserver_default()
    assert default_config.webserver_thread_pool_size == BeekeeperDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE
    assert default_config.notifications_endpoint == BeekeeperDefaults.DEFAULT_NOTIFICATIONS_ENDPOINT
    assert default_config.backtrace == BeekeeperDefaults.DEFAULT_BACKTRACE
    assert default_config.export_keys_wallet == BeekeeperDefaults.DEFAULT_EXPORT_KEYS_WALLET


async def test_default_values() -> None:
    """Test will check default values of Beekeeper."""
    # ARRANGE & ACT
    default_config = Beekeeper().generate_beekeepers_default_config()

    # ASSERT
    check_default_values_from_config(default_config)
