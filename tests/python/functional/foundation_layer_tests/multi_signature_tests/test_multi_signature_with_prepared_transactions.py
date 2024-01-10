import pytest

import test_tools as tt
from .test_multi_signature import create_post_comment_and_proposal
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation
from schemas.operations.create_claimed_account_operation import CreateClaimedAccountOperation
from schemas.operations.claim_account_operation import ClaimAccountOperation
from schemas.operations.remove_proposal_operation import RemoveProposalOperation
from schemas.operations.account_create_operation import AccountCreateOperation
from schemas.operations.transfer_operation import TransferOperation

from schemas.fields.compound import Authority


@pytest.mark.testnet()
def test_multi_signature_with_prepared_transactions(prepared_network: tuple[tt.ApiNode, tt.Wallet]) -> None:
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

    create_signed_multi_transaction(api_node_wallet, 100)
    only_initminer_wallet = tt.Wallet(attach_to=api_node, additional_arguments=["--transaction-serialization=hf26"])


def create_signed_multi_transaction(wallet: tt.Wallet, amount_of_prepared_transaction: int) -> list:
    transactions = []

    owner_key = tt.PublicKey("bob", secret="owner")
    active_key = tt.PublicKey("bob", secret="active")
    posting_key = tt.PublicKey("bob", secret="posting")
    memo_key = tt.PublicKey("bob", secret="memo")

    for num in range(amount_of_prepared_transaction):
        multi_transaction = (
            create_transaction_with_any_operation(
                wallet,
                RemoveProposalOperation(
                    proposal_owner="alice",
                    proposal_ids=[0],
                ),
                ClaimAccountOperation(
                    creator="alice",
                    fee=tt.Asset.Test(0),
                ),
                CreateClaimedAccountOperation(
                    creator="alice",
                    new_account_name="bob",
                    json_metadata=f"{num}",
                    owner=Authority(
                        weight_threshold=1,
                        account_auths=[],
                        key_auths=([[owner_key, 1]]),
                    ),
                    active=Authority(
                        weight_threshold=1,
                        account_auths=[],
                        key_auths=([[active_key, 1]]),
                    ),
                    posting=Authority(
                        weight_threshold=1,
                        account_auths=[],
                        key_auths=([[posting_key, 1]]),
                    ),
                    memo_key=memo_key,
                ),
                broadcast_transaction=False,
            ),
        )
        transactions.append(multi_transaction)

    return transactions
