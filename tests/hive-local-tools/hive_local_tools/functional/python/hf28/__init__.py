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
