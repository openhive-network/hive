from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for

# TESTY DLA ANDRZJEA, Bob ma wysłać transakcję alice w zróznymi wariantami tablic
KEY_HIERARCHY = {"posting": 0, "active": 1, "owner": 2}


# alice ustawia boba jako account_authoriu, a bobo wykonuje operacje za alice. Operacje muszą wymagać różnych typów authority
@run_for("testnet")
def test_account_authority(node: tt.InitNode):
    wallet = tt.Wallet(attach_to=node)
    accounts = {
        account: {"keys": generate_keys(account), "wallet": tt.Wallet(attach_to=node), "name": account}
        for account in ["alice", "bob"]
    }

    for account in accounts.values():
        wallet.api.create_account_with_keys(
            "initminer",
            account["name"],
            "",
            account["keys"]["posting"].public_key,
            account["keys"]["active"].public_key,
            account["keys"]["owner"].public_key,
            account["keys"]["memo"].public_key,
        )

        wallet.api.transfer_to_vesting("initminer", account["name"], tt.Asset.Test(100))
        wallet.api.transfer("initminer", account["name"], tt.Asset.Test(100), f"transfer hive to {account['name']}.")

        account["wallet"].api.import_keys(
            [
                account["keys"]["posting"].private_key,
                account["keys"]["active"].private_key,
                account["keys"]["owner"].private_key,
                account["keys"]["memo"].private_key,
            ]
        )

    # alice ustawia boba jako acc_auth (w posting, active, owner) -> bob może podpisywać się za alice kluczem posting
    accounts["alice"]["wallet"].api.update_account_auth_account("alice", "posting", "bob", 1)
    assert node.api.database.find_accounts(accounts=["alice"]).accounts[0]["posting"].account_auths[0][0] == "bob"

    accounts["bob"]["wallet"].api.use_authority("posting", "bob")
    accounts["bob"]["wallet"].api.post_comment("alice", "test-permlink", "", "someone0", "test-title", "body", "{}")

    # accounts["bob"]["wallet"].api.transfer("alice", "initminer", tt.Asset.Test(10), "bob signed alice transfer.")

    # accounts["bob"]["wallet"].api.use_authority("active", "alice")  # wallet alice ma korzystać z klucza active
    # accounts["alice"]["wallet"].api.post_comment("alice", "test-permlink", "", "someone0", "test-title", "this is a body", "{}")
    #
    # accounts["bob"]["wallet"].api.update_account("alice", "{}", owner=tt.Account("alice", secret="new-owner").public_key, memo=accounts["alice"]["keys"]["memo"].public_key, active=accounts["alice"]["keys"]["active"].public_key, posting=accounts["alice"]["keys"]["posting"].public_key)
    # print()


def generate_keys(account_name: str) -> dict:
    return {
        "posting": tt.Account(account_name, secret="posting"),
        "active": tt.Account(account_name, secret="active"),
        "owner": tt.Account(account_name, secret="owner"),
        "memo": tt.Account(account_name, secret="memo"),
    }


# post comment -> required post authorty
# accounts["alice"]["wallet"].api.post_comment("alice", "test-permlink", "", "someone0", "test-title", "this is a body", "{}")

# account_update -> zmiana klucza ownera
# accounts["alice"]["wallet"].api.update_account("alice", "{}", owner=tt.Account("alice", secret="new-owner").public_key, memo=accounts["alice"]["keys"]["memo"].public_key, active=accounts["alice"]["keys"]["active"].public_key, posting=accounts["alice"]["keys"]["posting"].public_key)
