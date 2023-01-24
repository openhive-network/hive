import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_appoint_a_proxy(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob")

    wallet.api.set_voting_proxy("alice", "bob")

    assert node.api.wallet_bridge.get_account("alice")["proxy"] == "bob"
    assert not all(node.api.wallet_bridge.get_account("bob")["proxied_vsf_votes"])

    node.wait_for_irreversible_block()
    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block timestamp: {timestamp}")

    # restart node with future date. The approval process takes 24 hours.
    node.restart(
        time_offset=tt.Time.serialize(tt.Time.parse(timestamp) + tt.Time.hours(24), format_=tt.Time.TIME_OFFSET_FORMAT))

    assert any(node.api.wallet_bridge.get_account("bob")["proxied_vsf_votes"])


@run_for("testnet")
def test_vote_power_value_after_proxy_removal(node):
    wallet = tt.Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for number in range(4):
            wallet.api.create_account('initminer', f"account-layer-{number}", '{}')
            wallet.api.transfer_to_vesting("initminer", f"account-layer-{number}", tt.Asset.Test(10 + number))

    with wallet.in_single_transaction():
        wallet.api.set_voting_proxy(f"account-layer-1", f"account-layer-0")
        wallet.api.set_voting_proxy(f"account-layer-2", f"account-layer-1")
        wallet.api.set_voting_proxy(f"account-layer-3", f"account-layer-2")

    # unbound the top of the proxy chain
    wallet.api.set_voting_proxy(f"account-layer-1", "")

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h")
    assert not all(node.api.wallet_bridge.get_account("account-layer-0")["proxied_vsf_votes"])


@run_for("testnet")
def test_sum_of_vesting_shares_on_first_layer_of_proxy(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice")
    wallet.create_account("bob", vests=tt.Asset.Test(111))
    wallet.create_account("carol", vests=tt.Asset.Test(222))

    wallet.api.set_voting_proxy("bob", "alice")
    wallet.api.set_voting_proxy("carol", "alice")

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h")

    bob_vesting_shares = int(node.api.wallet_bridge.get_account("bob")["vesting_shares"]["amount"])
    carol_vesting_shares = int(node.api.wallet_bridge.get_account("carol")["vesting_shares"]["amount"])

    assert node.api.wallet_bridge.get_account("alice")["proxied_vsf_votes"][0] == bob_vesting_shares + carol_vesting_shares


@run_for("testnet")
def test_too_long_proxy_chain(node):
    wallet = tt.Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for i in range(5):
            wallet.api.create_account('initminer', f"account-layer-{i}", '{}')
            wallet.api.transfer_to_vesting("initminer", f"account-layer-{i}", tt.Asset.Test(10+i))


    with wallet.in_single_transaction():
        for i in range(3):
            wallet.api.set_voting_proxy(f"account-layer-{i+1}", f"account-layer-{i}")

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy(f"account-layer-4", f"account-layer-3")
    error_response = exception.value.response['error']['message']
    assert "Proxy chain is too long." in error_response


@run_for("testnet")
def test_proxy_change(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob", vests=tt.Asset.Test(2))
    wallet.create_account("carol", vests=tt.Asset.Test(3))

    wallet.api.set_voting_proxy("alice", "bob")
    alice_vesting_shares = node.api.wallet_bridge.get_account("alice")["vesting_shares"]["amount"]
    assert node.api.wallet_bridge.get_account("alice")["proxy"] == "bob"

    wallet.api.set_voting_proxy("alice", "carol")
    assert node.api.wallet_bridge.get_account("alice")["proxy"] == "carol"

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h")
    assert node.api.wallet_bridge.get_account("carol")["proxied_vsf_votes"][0] == int(alice_vesting_shares)


@run_for("testnet")
def test_set_the_proxy_on_the_same_account_twice(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=tt.Asset.Test(1))
    wallet.create_account("bob")

    wallet.api.set_voting_proxy("alice", "bob")

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy("alice", "bob")

    error_response = exception.value.response['error']['message']
    assert "Proxy must change." in error_response


@run_for("testnet")
def test_long_single_layer_proxy_chain(node):
    wallet = tt.Wallet(attach_to=node)

    number_of_accounts = 10_000
    step = 1000

    accounts = wallet.create_accounts(number_of_accounts)

    for num in range(0, len(accounts), step):
        with wallet.in_single_transaction():
            for account in accounts[num: num + step]:
                wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(10))
        with wallet.in_single_transaction():
            for n, account in enumerate(accounts[num: num + step]):
                if account != accounts[-1]:
                    wallet.api.set_voting_proxy(accounts[n + num].name, accounts[n + num + 1].name)

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h")

    for account_number, account in enumerate(accounts[:-1]):
        assert node.api.wallet_bridge.get_account(accounts[account_number].name)["proxy"]\
               == node.api.wallet_bridge.get_account(accounts[account_number + 1].name)["name"]


@run_for("testnet")
def test_vesting_shares_values_on_four_proxy_layers(node):
    node.restart(time_offset="+0 x5")
    wallet = tt.Wallet(attach_to=node)

    accounts = wallet.create_accounts(5)

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(1 + ord(account.name[-1])))

    with wallet.in_single_transaction():
        for num in range(len(accounts[:-1])):
            wallet.api.set_voting_proxy(f"account-{num}", f"account-{num+1}")

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h")

    vesting_shares = []
    for account_number in range(len(accounts) - 1):
        vs_amount = int(node.api.wallet_bridge.get_account(f"account-{account_number}")["vesting_shares"]["amount"])
        vs_value_on_position = node.api.wallet_bridge.get_account(f"account-{account_number + 1}")["proxied_vsf_votes"][0]
        assert vs_amount == vs_value_on_position
        vesting_shares.insert(0, vs_amount)

    assert node.api.wallet_bridge.get_account("account-4")["proxied_vsf_votes"] == vesting_shares
