import pytest
import test_tools as tt
from ..conftest import UpdateAccount


@pytest.fixture
def alice(prepared_node, wallet):
    # slow down node - speeding up time caused random fails (it's not possible to do "+0h x1")
    prepared_node.restart(time_offset="+1h")
    # wallet.create_account creates account with 4 the same keys which is not wanted in this kind of tests
    wallet.api.create_account_with_keys(
        "initminer",
        "alice",
        "{}",
        tt.Account("alice", secret="owner_key").public_key,
        tt.Account("alice", secret="active_key").public_key,
        tt.Account("alice", secret="posting_key").public_key,
        tt.Account("alice", secret="memo_key").public_key,
        broadcast=True,
    )

    wallet.api.import_keys(
        [
            tt.Account("alice", secret="owner_key").private_key,
            tt.Account("alice", secret="active_key").private_key,
            tt.Account("alice", secret="posting_key").private_key,
            tt.Account("alice", secret="memo_key").private_key,
        ]
    )

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50), broadcast=True)
    return UpdateAccount("alice", prepared_node, wallet)
