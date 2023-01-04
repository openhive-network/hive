import test_tools as tt


def create_account_and_create_order(wallet, account_name):
    wallet.api.create_account("initminer", account_name, "{}")
    wallet.api.transfer("initminer", account_name, tt.Asset.Test(100), "memo")
    wallet.api.transfer_to_vesting("initminer", account_name, tt.Asset.Test(100))
    wallet.api.create_order(account_name, 1000, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)


def create_accounts_with_vests_and_tbd(wallet, accounts):
    created_accounts = wallet.create_accounts(len(accounts))
    assert get_accounts_name(created_accounts) == accounts

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting("initminer", account, tt.Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer("initminer", account, tt.Asset.Tbd(10000), "memo")


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def prepare_proposals(wallet, accounts):
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(account, "permlink", "", "parent-permlink", "title", "body", "{}")

    with wallet.in_single_transaction():
        for account_number in range(len(accounts)):
            wallet.api.create_proposal(
                accounts[account_number],
                accounts[account_number],
                tt.Time.from_now(weeks=10),
                tt.Time.from_now(weeks=15),
                tt.Asset.Tbd(account_number * 100),
                f"subject-{account_number}",
                "permlink",
            )
