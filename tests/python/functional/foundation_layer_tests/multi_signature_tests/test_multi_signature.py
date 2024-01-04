from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed

import pytest
from helpy.exceptions import RequestError

import test_tools as tt

spam_transactions: bool = True


@pytest.mark.testnet()
def test_multi_signature_with_missing_signature(prepared_network: tuple[tt.ApiNode, tt.Wallet]) -> None:
    api_node, api_node_wallet = prepared_network

    api_node_wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(10))

    signed_transaction = api_node_wallet.api.transfer("alice", "initminer", tt.Asset.Test(100), "memo", broadcast=False)

    signed_transaction["signatures"].pop(-1)
    assert len(signed_transaction["signatures"]) == 1600 - 1, "Incorrect number of signatures."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_transaction)

    assert "Missing Active Authority" in exception.value.error


@pytest.mark.testnet()
def test_multi_signature_with_additional_signature(prepared_network: tuple[tt.ApiNode, tt.Wallet]) -> None:
    api_node, api_node_wallet = prepared_network

    additional_sign = api_node_wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(10))["signatures"][0]

    signed_transaction = api_node_wallet.api.transfer("alice", "initminer", tt.Asset.Test(100), "memo", broadcast=False)

    signed_transaction["signatures"].append(additional_sign)
    assert len(signed_transaction["signatures"]) == 1600 + 1, "Incorrect number of signatures."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_transaction)

    assert "Unnecessary signature(s) detected" in exception.value.error


@pytest.mark.testnet()
def test_multi_signature_with_incorrect_signature(prepared_network: tuple[tt.ApiNode, tt.Wallet]) -> None:
    api_node, api_node_wallet = prepared_network

    additional_sign = api_node_wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(10))["signatures"][0]

    signed_transaction = api_node_wallet.api.transfer("alice", "initminer", tt.Asset.Test(100), "memo", broadcast=False)
    original_last_signature = signed_transaction["signatures"][-1]

    signed_transaction["signatures"][-1] = additional_sign
    assert original_last_signature != signed_transaction["signatures"][-1], "The signature has not been changed."
    assert len(signed_transaction["signatures"]) == 1600, "Incorrect number of signatures."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_transaction)

    assert "Missing Active Authority" in exception.value.error


@pytest.mark.testnet()
def test_multi_signature_without_sufficient_resource_credits(prepared_network: tuple[tt.ApiNode, tt.Wallet]) -> None:
    api_node, api_node_wallet = prepared_network

    signed_transaction = api_node_wallet.api.transfer("alice", "initminer", tt.Asset.Test(10), "memo", broadcast=False)
    assert len(signed_transaction["signatures"]) == 1600, "Incorrect number of signatures."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_transaction)

    assert "payer has not enough RC mana for transaction" in exception.value.error


@pytest.mark.testnet()
def test_multi_signature_without_sufficient_resource_credits_for_costly_multi_transactions(
    prepared_network: tuple[tt.ApiNode, tt.Wallet]
) -> None:
    api_node, api_node_wallet = prepared_network

    api_node_wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "memo")
    api_node_wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(0.01))

    create_post_comment_and_proposal(api_node_wallet, "alice")

    voters = api_node_wallet.create_accounts(1000, "voter")

    with api_node_wallet.in_single_transaction():
        for voter in voters:
            api_node_wallet.api.transfer_to_vesting("initminer", voter.name, tt.Asset.Test(1))
            api_node_wallet.api.update_proposal_votes(voter=voter.name, proposals=[0], approve=True)

    with api_node_wallet.in_single_transaction(broadcast=False) as transaction:
        api_node_wallet.api.remove_proposal(deleter="alice", ids=[0])
        api_node_wallet.api.claim_account_creation(creator="alice", fee=tt.Asset.Test(0))
        api_node_wallet.api.create_funded_account_with_keys(
            creator="alice",
            new_account_name="bob",
            initial_amount=tt.Asset.Test(0),
            memo="memo",
            json_meta="{}",
            owner_key=tt.PublicKey("bob", secret="owner"),
            active_key=tt.PublicKey("bob", secret="active"),
            posting_key=tt.PublicKey("bob", secret="posting"),
            memo_key=tt.PublicKey("bob", secret="memo"),
        )

    signed_costly_transaction = transaction.get_response()
    assert len(signed_costly_transaction["operations"]) == 3, "Incorrect number of operations in a transaction."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_costly_transaction)
    assert "payer has not enough RC mana for transaction" in exception.value.error


@pytest.mark.skip()
@pytest.mark.testnet()
def test_multi_signature_without_sufficient_resource_credits_for_costly_multi_transactions_on_multi_threads(
    prepared_network: tuple[tt.ApiNode, tt.Wallet]
) -> None:
    api_node, api_node_wallet = prepared_network

    with api_node_wallet.in_single_transaction():
        api_node_wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(100), "memo")
        api_node_wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(0.01))

    create_post_comment_and_proposal(api_node_wallet, "alice")

    voters = api_node_wallet.create_accounts(1000, "voter")

    with api_node_wallet.in_single_transaction():
        for voter in voters:
            api_node_wallet.api.transfer_to_vesting("initminer", voter.name, tt.Asset.Test(1))
            api_node_wallet.api.update_proposal_votes(voter=voter.name, proposals=[0], approve=True)

    with api_node_wallet.in_single_transaction(broadcast=False) as transaction:
        api_node_wallet.api.remove_proposal(deleter="alice", ids=[0])
        api_node_wallet.api.claim_account_creation(creator="alice", fee=tt.Asset.Test(0))
        api_node_wallet.api.create_funded_account_with_keys(
            creator="alice",
            new_account_name="bob",
            initial_amount=tt.Asset.Test(0),
            memo="memo",
            json_meta="{}",
            owner_key=tt.PublicKey("bob", secret="owner"),
            active_key=tt.PublicKey("bob", secret="active"),
            posting_key=tt.PublicKey("bob", secret="posting"),
            memo_key=tt.PublicKey("bob", secret="memo"),
        )

    signed_costly_transaction = transaction.get_response()
    assert len(signed_costly_transaction["operations"]) == 3, "Incorrect number of operations in a transaction."

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        api_node.api.wallet_bridge.broadcast_transaction_synchronous(signed_costly_transaction)
    assert "payer has not enough RC mana for transaction" in exception.value.error

    only_initminer_wallet = tt.Wallet(attach_to=api_node, additional_arguments=["--transaction-serialization=hf26"])

    tt.logger.info("Starting spamming with lots of transactions...")
    with ThreadPoolExecutor() as executor:
        tasks = []
        for num in range(100):
            if num == 0:
                tasks.extend(
                    (
                        executor.submit(send_transfer, api_node, only_initminer_wallet),
                        executor.submit(check_account_balance, api_node, "alice"),
                    )
                )
            else:
                tasks.append(executor.submit(silent_broadcast, api_node, signed_costly_transaction))

        for future in as_completed(tasks):
            future.result()

    alice_balance = api_node.api.database.find_accounts(accounts=["alice"]).accounts[0].balance
    tt.logger.info(f"alice balance: {alice_balance}")
    assert alice_balance >= tt.Asset.Test(500)


def check_account_balance(node: tt.InitNode | tt.ApiNode, account_name: str) -> bool:
    global spam_transactions

    while spam_transactions:
        if node.api.database.find_accounts(accounts=[account_name]).accounts[0].balance > tt.Asset.Test(500):
            spam_transactions = False
        node.wait_number_of_blocks(1)


def create_post_comment_and_proposal(wallet: tt.Wallet, account_name: str) -> None:
    wallet.api.post_comment(account_name, "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}")
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal(
        creator=account_name,
        receiver=account_name,
        start_date=tt.Time.now(),
        end_date=tt.Time.from_now(days=2),
        daily_pay=tt.Asset.Tbd(1),
        subject="test subject",
        permlink="test-permlink",
    )


def send_transfer(node: tt.InitNode, wallet: tt.Wallet) -> None:
    global spam_transactions
    counter = 0

    while spam_transactions:
        tt.logger.info(f"send-transfer-{counter}")
        counter += 1
        transaction = wallet.api.transfer("initminer", "alice", tt.Asset.Test(1), f"message-{counter}", broadcast=False)
        node.api.network_broadcast.broadcast_transaction(trx=transaction)


def silent_broadcast(node: tt.InitNode, transaction: dict) -> None:
    """Allows to spam the transaction despite receiving an exception"""
    global spam_transactions

    while spam_transactions:
        try:
            node.api.network_broadcast.broadcast_transaction(trx=transaction)
        except RequestError:  # noqa: PERF203
            continue
