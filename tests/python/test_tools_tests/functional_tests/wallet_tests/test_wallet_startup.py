from __future__ import annotations

from typing import TYPE_CHECKING, Any

import pytest
import test_tools as tt

if TYPE_CHECKING:
    from collections.abc import Callable


def test_attaching_wallet_to_local_node(node: tt.InitNode) -> None:
    tt.Wallet(attach_to=node)


def test_attaching_wallet_to_remote_node() -> None:
    # Connecting with real remote node (e.g. from main net) is not reliable,
    # so wallet is connected to local node, but via remote node interface.
    local_node = tt.InitNode()
    local_node.run()

    remote_node = tt.RemoteNode(local_node.http_endpoint, ws_endpoint=local_node.ws_endpoint)

    tt.Wallet(attach_to=remote_node)


def test_attaching_wallet_to_not_run_node() -> None:
    node = tt.InitNode()

    with pytest.raises(tt.exceptions.NodeIsNotRunningError):
        tt.Wallet(attach_to=node)


def test_offline_mode_startup() -> None:
    tt.Wallet()


def test_restart(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.restart()


def __restart_wallet_manually(wallet: tt.Wallet) -> None:
    wallet.close()
    wallet.run()


@pytest.mark.parametrize(
    "restart",
    [
        __restart_wallet_manually,
        lambda wallet: wallet.restart,
    ],
)
def test_if_keys_are_stored_after_restart(restart: Callable[..., None] | Callable[..., Any], node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    assert len(wallet.api.list_keys()) == 1  # After start only initminer key is registered

    wallet.api.create_account("initminer", "alice", "")
    assert len(wallet.api.list_keys()) == 5  # After account creation 4 new keys were register  # noqa: PLR2004

    restart(wallet)

    assert len(wallet.api.list_keys()) == 5  # After restart keys still should be register  # noqa: PLR2004


def test_if_keys_are_stored_after_together_wallet_and_node_restart(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    assert len(wallet.api.list_keys()) == 1  # After start only initminer key is registered

    wallet.api.create_account("initminer", "alice", "")
    assert len(wallet.api.list_keys()) == 5  # After account creation 4 new keys were register  # noqa: PLR2004

    wallet.close()
    node.close()

    node.run()
    wallet.run()

    assert len(wallet.api.list_keys()) == 5  # After restart keys still should be register  # noqa: PLR2004
