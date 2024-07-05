from __future__ import annotations

import test_tools as tt


def wait_for_current_hardfork(node: tt.InitNode, current_hardfork_number: int) -> None:
    def is_current_hardfork() -> bool:
        version = node.api.wallet_bridge.get_hardfork_version()
        return int(version.split(".")[1]) >= current_hardfork_number

    tt.logger.info("Wait for current hardfork...")
    tt.Time.wait_for(is_current_hardfork)
    tt.logger.info(f"Current Hardfork {current_hardfork_number} applied.")
