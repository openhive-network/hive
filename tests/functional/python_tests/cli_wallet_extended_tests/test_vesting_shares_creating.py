import pytest

import test_tools as tt


def test_delegate(node, wallet):
    wallet.api.create_account("initminer", "alice", "{}")

    wallet.api.transfer("initminer", "alice", tt.Asset.Test(200), "avocado")

    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "banana")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50))

    wallet.api.create_account("alice", "bob", "{}")

    wallet.api.transfer("alice", "bob", tt.Asset.Test(50), "lemon")

    wallet.api.transfer_to_vesting("alice", "bob", tt.Asset.Test(25))

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(1.123456))

    _result = wallet.api.get_account("alice")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(1.123456)
    assert _result["balance"] == tt.Asset.Test(125)
    assert _result["hbd_balance"] == tt.Asset.Tbd(100)

    _result = wallet.api.get_account("bob")
    assert _result["received_vesting_shares"] == tt.Asset.Vest(1.123456)
    assert _result["balance"] == tt.Asset.Test(50)
    assert _result["hbd_balance"] == tt.Asset.Tbd(0)

    wallet.api.delegate_vesting_shares_and_transfer("alice", "bob", tt.Asset.Vest(1), tt.Asset.Tbd(6.666), "watermelon")

    _result = wallet.api.get_account("alice")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(1.123456)
    assert _result["balance"] == tt.Asset.Test(125)
    assert _result["hbd_balance"] == tt.Asset.Tbd(93.334)

    _result = wallet.api.get_account("bob")
    assert _result["received_vesting_shares"] == tt.Asset.Vest(1)
    assert _result["balance"] == tt.Asset.Test(50)
    assert _result["hbd_balance"] == tt.Asset.Tbd(6.666)

    wallet.api.delegate_vesting_shares_nonblocking("bob", "alice", tt.Asset.Vest(0.1))

    tt.logger.info("Waiting...")
    node.wait_number_of_blocks(1)

    _result = wallet.api.get_account("alice")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(1.123456)
    assert _result["received_vesting_shares"] == tt.Asset.Vest(0.1)
    assert _result["balance"] == tt.Asset.Test(125)
    assert _result["hbd_balance"] == tt.Asset.Tbd(93.334)

    _result = wallet.api.get_account("bob")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(0.1)
    assert _result["received_vesting_shares"] == tt.Asset.Vest(1)
    assert _result["balance"] == tt.Asset.Test(50)
    assert _result["hbd_balance"] == tt.Asset.Tbd(6.666)

    wallet.api.delegate_vesting_shares_and_transfer_nonblocking(
        "bob", "alice", tt.Asset.Vest(0.1), tt.Asset.Tbd(6.555), "pear"
    )

    tt.logger.info("Waiting...")
    node.wait_number_of_blocks(1)

    _result = wallet.api.get_account("alice")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(1.123456)
    assert _result["received_vesting_shares"] == tt.Asset.Vest(0.1)
    assert _result["balance"] == tt.Asset.Test(125)
    assert _result["hbd_balance"] == tt.Asset.Tbd(99.889)

    _result = wallet.api.get_account("bob")
    assert _result["delegated_vesting_shares"] == tt.Asset.Vest(0.1)
    assert _result["received_vesting_shares"] == tt.Asset.Vest(1)
    assert _result["balance"] == tt.Asset.Test(50)
    assert _result["hbd_balance"] == tt.Asset.Tbd(0.111)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.claim_reward_balance("initminer", tt.Asset.Test(0), tt.Asset.Tbd(0), tt.Asset.Vest(0.000001))

    response = exception.value.response
    assert "Cannot claim that much VESTS" in response["error"]["message"]
