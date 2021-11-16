from test_tools import Wallet


def test_info_function(wallet: Wallet):
    assert len(wallet.api.info()) == 54, "Info have incorrect data set"
