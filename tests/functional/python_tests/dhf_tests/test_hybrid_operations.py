from beem import Hive
from beem.account import Account
from beem.transactionbuilder import TransactionBuilder
import beemapi
from beembase import operations
import pytest
import test_tools as tt

from .conftest import CREATOR
from ... import hive_utils


def transfer_assets_to_accounts(node, from_account, accounts, amount, asset, wif=None):
    for account in accounts:
        tt.logger.info("Transfer from {} to {} amount {} {}".format(from_account, account["name"], amount, asset))
        acc = Account(from_account, hive_instance=node)
        acc.transfer(account["name"], amount, asset, memo="initial transfer")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_to_vesting(node, from_account, accounts, amount, asset):
    for account in accounts:
        tt.logger.info(
            "Transfer to vesting from {} to {} amount {} {}".format(from_account, account["name"], amount, asset)
        )
        acc = Account(from_account, hive_instance=node)
        acc.transfer_to_vesting(amount, to=account["name"], asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def test_hybrid_operations(node):
    accounts = [
        # place accounts here in the format: {'name' : name, 'private_key' : private-key, 'public_key' : public-key}
        {
            "name": "tester001",
            "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa",
            "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J",
        },
    ]

    wif = tt.Account("initminer").private_key
    node_url = f"http://{node.http_endpoint}"
    keys = [wif]

    for account in accounts:
        keys.append(account["private_key"])

    node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

    tt.logger.info("Chain prefix is: {}".format(node_client.prefix))
    tt.logger.info("Chain ID is: {}".format(node_client.get_config()["HIVE_CHAIN_ID"]))

    # create test account
    tt.logger.info("Creating account: {}".format(accounts[0]["name"]))
    node_client.create_account(
        accounts[0]["name"],
        owner_key=accounts[0]["public_key"],
        active_key=accounts[0]["public_key"],
        posting_key=accounts[0]["public_key"],
        memo_key=accounts[0]["public_key"],
        store_keys=False,
        creator=CREATOR,
        asset="TESTS",
    )
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

    transfer_to_vesting(node_client, CREATOR, accounts, "300.000", "TESTS")

    transfer_assets_to_accounts(
        node_client,
        CREATOR,
        accounts,
        "400.000",
        "TESTS",
    )

    transfer_assets_to_accounts(
        node_client,
        CREATOR,
        accounts,
        "400.000",
        "TBD",
    )

    # create comment
    tt.logger.info(
        "New post ==> ({},{},{},{},{})".format(
            "Hivepy proposal title [{}]".format(accounts[0]["name"]),
            "Hivepy proposal body [{}]".format(accounts[0]["name"]),
            accounts[0]["name"],
            "hivepy-proposal-title-{}".format(accounts[0]["name"]),
            "proposals",
        )
    )

    node_client.post(
        "Hivepy proposal title [{}]".format(accounts[0]["name"]),
        "Hivepy proposal body [{}]".format(accounts[0]["name"]),
        accounts[0]["name"],
        permlink="hivepy-proposal-title-{}".format(accounts[0]["name"]),
        tags="firstpost",
    )
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

    # use hybrid op with old keys
    tt.logger.info("Using hybrid op with old keys")
    with pytest.raises(beemapi.exceptions.UnhandledRPCError) as exception:
        tx = TransactionBuilder(hive_instance=node_client)
        ops = []
        op = operations.Comment_options(
            **{
                "author": accounts[0]["name"],
                "permlink": "hivepy-proposal-title-{}".format(accounts[0]["name"]),
                "max_accepted_payout": "1000.000 TBD",
                "percent_steem_dollars": 5000,
                "allow_votes": True,
                "allow_curation_rewards": True,
                "prefix": node_client.prefix,
            }
        )
        ops.append(op)
        tx.appendOps(ops)
        tx.appendWif(accounts[0]["private_key"])
        tx.sign()
        tx.broadcast()
        tt.logger.exception("Expected exception for old style op was not thrown")
        assert "Assert Exception:false: Obsolete form of transaction detected, update your wallet." in exception.value

    hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)

    # use hybrid op with new keys
    tt.logger.info("Using hybrid op with new keys")
    tx = TransactionBuilder(hive_instance=node_client)
    ops = []
    op = operations.Comment_options(
        **{
            "author": accounts[0]["name"],
            "permlink": "hivepy-proposal-title-{}".format(accounts[0]["name"]),
            "max_accepted_payout": "1000.000 TBD",
            "percent_hbd": 5000,
            "allow_votes": True,
            "allow_curation_rewards": True,
            "prefix": node_client.prefix,
        }
    )
    ops.append(op)
    tx.appendOps(ops)
    tx.appendWif(accounts[0]["private_key"])
    tx.sign()
    tx.broadcast()

    hive_utils.common.wait_n_blocks(node_client.rpc.url, 5)
