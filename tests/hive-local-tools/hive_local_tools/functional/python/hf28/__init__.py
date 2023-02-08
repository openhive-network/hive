import test_tools as tt


def create_proposal(wallet):
    wallet.api.post_comment("initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}")
    # create proposal with permlink pointing to comment
    wallet.api.create_proposal("initminer",
                               "initminer",
                               tt.Time.now(),
                               tt.Time.from_now(weeks=1),
                               tt.Asset.Tbd(5),
                               "test subject",
                               "test-permlink")


def post_comment(wallet, number_of_comments: int):
    accounts = wallet.create_accounts(number_of_accounts=number_of_comments, name_base="creator")

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account.name, tt.Asset.Test(1))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(f"{account.name}", f"comment-of-{account.name}", "", "someone0",
                                    "test-title", "this is a body", "{}")
