from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_appoint_a_proxy(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob")

    wallet.api.set_voting_proxy("alice", "bob")

    assert (alice_account := node.api.wallet_bridge.get_account("alice")) is not None
    assert alice_account.proxy == "bob"

    assert (bob_account := node.api.wallet_bridge.get_account("bob")) is not None
    assert not all(bob_account.proxied_vsf_votes)

    node.wait_for_irreversible_block()
    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num).block.timestamp
    tt.logger.info(f"head block timestamp: {timestamp}")

    # restart node with future date. The approval process takes 24 hours.
    node.restart(time_control=tt.StartTimeControl(start_time=tt.Time.parse(timestamp) + tt.Time.hours(24)))

    assert (response := node.api.wallet_bridge.get_account("bob")) is not None
    assert any(response.proxied_vsf_votes)


@run_for("testnet")
def test_vote_power_value_after_proxy_removal(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for number in range(4):
            wallet.api.create_account("initminer", f"account-layer-{number}", "{}")
            wallet.api.transfer_to_vesting("initminer", f"account-layer-{number}", tt.Asset.Test(10 + number))

    with wallet.in_single_transaction():
        wallet.api.set_voting_proxy("account-layer-1", "account-layer-0")
        wallet.api.set_voting_proxy("account-layer-2", "account-layer-1")
        wallet.api.set_voting_proxy("account-layer-3", "account-layer-2")

    # unbound the top of the proxy chain
    wallet.api.set_voting_proxy("account-layer-1", "")

    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))

    assert (response := node.api.wallet_bridge.get_account("account-layer-0")) is not None
    assert not all(response.proxied_vsf_votes)


@run_for("testnet")
def test_sum_of_vesting_shares_on_first_layer_of_proxy(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice")
    wallet.create_account("bob", vests=tt.Asset.Test(111))
    wallet.create_account("carol", vests=tt.Asset.Test(222))

    wallet.api.set_voting_proxy("bob", "alice")
    wallet.api.set_voting_proxy("carol", "alice")

    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))

    assert (bob := node.api.wallet_bridge.get_account("bob")) is not None
    bob_vesting_shares = int(bob.vesting_shares.amount)

    assert (carol := node.api.wallet_bridge.get_account("carol")) is not None
    carol_vesting_shares = int(carol.vesting_shares.amount)

    assert (alice := node.api.wallet_bridge.get_account("alice")) is not None
    assert alice.proxied_vsf_votes[0] == bob_vesting_shares + carol_vesting_shares


@run_for("testnet")
def test_too_long_proxy_chain(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for i in range(5):
            wallet.api.create_account("initminer", f"account-layer-{i}", "{}")
            wallet.api.transfer_to_vesting("initminer", f"account-layer-{i}", tt.Asset.Test(10 + i))

    with wallet.in_single_transaction():
        for i in range(3):
            wallet.api.set_voting_proxy(f"account-layer-{i + 1}", f"account-layer-{i}")

    with pytest.raises(ErrorInResponseError) as exception:
        wallet.api.set_voting_proxy("account-layer-4", "account-layer-3")
    assert "Proxy chain is too long." in exception.value.error


@run_for("testnet")
def test_proxy_change(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob", vests=tt.Asset.Test(2))
    wallet.create_account("carol", vests=tt.Asset.Test(3))

    wallet.api.set_voting_proxy("alice", "bob")

    assert (alice := node.api.wallet_bridge.get_account("alice")) is not None
    alice_vesting_shares = alice.vesting_shares.amount
    assert alice.proxy == "bob"

    wallet.api.set_voting_proxy("alice", "carol")
    assert (alice := node.api.wallet_bridge.get_account("alice")) is not None
    assert alice.proxy == "carol"

    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))
    assert (carol := node.api.wallet_bridge.get_account("carol")) is not None
    assert carol.proxied_vsf_votes[0] == int(alice_vesting_shares)


@run_for("testnet")
def test_set_the_proxy_on_the_same_account_twice(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob")

    wallet.api.set_voting_proxy("alice", "bob")

    with pytest.raises(ErrorInResponseError) as exception:
        wallet.api.set_voting_proxy("alice", "bob")

    assert "Proxy must change." in exception.value.error


@run_for("testnet")
def test_long_single_layer_proxy_chain(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.set_transaction_expiration(seconds=240)

    number_of_accounts = 10_000
    step = 1000

    accounts = wallet.create_accounts(number_of_accounts)

    for num in range(0, len(accounts), step):
        with wallet.in_single_transaction():
            for account in accounts[num : num + step]:
                wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(10))
        with wallet.in_single_transaction():
            for n, account in enumerate(accounts[num : num + step]):
                if account != accounts[-1]:
                    wallet.api.set_voting_proxy(accounts[n + num].name, accounts[n + num + 1].name)

    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))

    for account_number, _ in enumerate(accounts[:-1]):
        assert (account := node.api.wallet_bridge.get_account(accounts[account_number].name)) is not None
        assert account.proxy == accounts[account_number + 1].name


@run_for("testnet")
def test_vesting_shares_values_on_four_proxy_layers(node: tt.InitNode) -> None:
    node.restart(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)

    accounts = wallet.create_accounts(5)

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(1 + ord(account.name[-1])))

    with wallet.in_single_transaction():
        for num in range(len(accounts[:-1])):
            wallet.api.set_voting_proxy(f"account-{num}", f"account-{num + 1}")

    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))

    vesting_shares = []
    for account_number in range(len(accounts) - 1):
        assert (account_0 := node.api.wallet_bridge.get_account(f"account-{account_number}")) is not None
        vs_amount = int(account_0.vesting_shares.amount)

        assert (account_1 := node.api.wallet_bridge.get_account(f"account-{account_number + 1}")) is not None
        vs_value_on_position = account_1.proxied_vsf_votes[0]
        assert vs_amount == vs_value_on_position
        vesting_shares.insert(0, vs_amount)

    assert (account_4 := node.api.wallet_bridge.get_account("account-4")) is not None
    assert account_4.proxied_vsf_votes == vesting_shares
