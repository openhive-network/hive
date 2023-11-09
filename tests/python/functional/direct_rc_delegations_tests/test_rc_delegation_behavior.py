from __future__ import annotations

import math
from concurrent.futures import ThreadPoolExecutor

import pytest

import test_tools as tt


def test_delegated_rc_account_execute_operation(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], "alice", "{}")


def test_undelegated_rc_account_reject_execute_operation(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(0.1))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    wallet.api.create_account(accounts[1], "alice", "{}")

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 0)

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.create_account(accounts[1], "bob", "{}")


def test_rc_delegation_to_same_receiver(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(3, "receiver"))

    with wallet.in_single_transaction():
        wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))
        wallet.api.transfer_to_vesting("initminer", accounts[1], tt.Asset.Test(10))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc(accounts[0], [accounts[2]], 10)
        wallet.api.delegate_rc(accounts[1], [accounts[2]], 2)

    assert get_rc_account_info(accounts[2], wallet)["max_rc"] == 12


def test_same_rc_delegation_rejection(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)

    with pytest.raises(tt.exceptions.CommunicationError):
        # Can not make same delegation RC two times
        wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)


def test_overwriting_of_delegated_rc_value(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
    assert get_rc_account_info(accounts[1], wallet)["max_rc"] == 5

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    assert get_rc_account_info(accounts[1], wallet)["max_rc"] == 10

    wallet.api.delegate_rc(accounts[0], [accounts[1]], 7)
    assert get_rc_account_info(accounts[1], wallet)["max_rc"] == 7


def test_large_rc_delegation(node: tt.InitNode, wallet: tt.Wallet) -> None:
    # Wait for block is necessary because transactions must always enter the same specific blocks.
    # This way, 'cost of transaction' is always same and is possible to delegate maximal, huge amount of RC.
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    cost_of_transaction = 150117
    node.wait_for_block_with_number(3)
    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(200000000))
    rc_to_delegate = int(get_rc_account_info(accounts[0], wallet)["rc_manabar"]["current_mana"]) - cost_of_transaction
    wallet.api.delegate_rc(accounts[0], [accounts[1]], rc_to_delegate)
    assert int(get_rc_account_info(accounts[1], wallet)["max_rc"]) == rc_to_delegate


def test_out_of_int64_rc_delegation(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(2000))

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], 9223372036854775808)


def test_reject_of_delegation_of_delegated_rc(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(3, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[1], [accounts[2]], 50)


def test_wrong_sign_in_transaction(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))

    operation = wallet.api.delegate_rc(accounts[0], [accounts[1]], 100, broadcast=False)
    operation["operations"][0][1]["required_posting_auths"][0] = accounts[1]

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.sign_transaction(operation)


def test_minus_rc_delegation(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delegate_rc(accounts[0], [accounts[1]], -100)


def test_power_up_delegator(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(2, "receiver"))

    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 100)
    rc0 = get_rc_account_info(accounts[1], wallet)["rc_manabar"]["current_mana"]
    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(100))
    rc1 = get_rc_account_info(accounts[1], wallet)["rc_manabar"]["current_mana"]
    assert rc0 == rc1


@pytest.mark.node_shared_file_size("16G")
def test_multidelegation(wallet: tt.Wallet) -> None:
    amount_of_delegated_rc = 1
    number_of_threads = 50
    tt.logger.info("Start of delegators and receivers creation")
    accounts = get_accounts_name(wallet.create_accounts(100_000, "receiver"))
    delegators = get_accounts_name(wallet.create_accounts(number_of_threads, "delegator"))
    tt.logger.info("End of delegators and receivers creation")
    number_of_delegations_in_thread = math.ceil(len(accounts) / number_of_threads)

    with wallet.in_single_transaction():
        for thread_number in range(number_of_threads):
            wallet.api.transfer_to_vesting("initminer", delegators[thread_number], tt.Asset.Test(0.1))

    tasks_list = []
    executor = ThreadPoolExecutor(max_workers=number_of_threads)
    for thread_number in range(number_of_threads):
        tasks_list.append(
            executor.submit(
                delegate_rc,
                wallet,
                delegators[thread_number],
                accounts[
                    thread_number * number_of_delegations_in_thread : thread_number * number_of_delegations_in_thread
                    + number_of_delegations_in_thread
                ],
                amount_of_delegated_rc,
            )
        )

    for thread_number in tasks_list:
        thread_number.result()

    for account_index in [0, int(len(accounts) / 2), -1]:
        assert get_rc_account_info(accounts[account_index], wallet)["received_delegated_rc"] == amount_of_delegated_rc


def test_delegations_cancellation_after_rollback_vest_delegation_to_delegator(wallet: tt.Wallet) -> None:
    accounts = get_accounts_name(wallet.create_accounts(5, "account"))
    wallet.api.transfer_to_vesting("initminer", accounts[0], tt.Asset.Test(10000))
    wallet.api.delegate_vesting_shares(accounts[0], accounts[1], tt.Asset.Vest(1000))
    wallet.api.delegate_rc(accounts[1], accounts[2:5], 10)

    assert len(wallet.api.list_rc_direct_delegations([accounts[1], accounts[0]], 100)) == 3

    wallet.api.delegate_vesting_shares(accounts[0], accounts[1], tt.Asset.Vest(0))

    assert len(wallet.api.list_rc_direct_delegations([accounts[1], accounts[0]], 100)) == 0


def delegate_rc(wallet: tt.Wallet, delegator: str, receivers: list, amount_of_delegated_rc: int) -> None:
    tt.logger.info(f"Delegation accounts from range {receivers[0]} : {receivers[-1]}--------START")
    for account_number in range(0, len(receivers), 100):
        wallet.api.delegate_rc(delegator, receivers[account_number : account_number + 100], amount_of_delegated_rc)
    tt.logger.info(f"Delegation accounts from range {receivers[0]} : {receivers[-1]}--------END")


def get_accounts_name(accounts: list) -> list:
    accounts_names = []
    for account_number in range(len(accounts)):
        accounts_names.append(accounts[account_number].name)
    return accounts_names


def get_rc_account_info(account: str, wallet: tt.Wallet) -> dict:
    return wallet.api.find_rc_accounts([account])[0]
