import test_tools as tt


def test_info_function(wallet: tt.Wallet):
    assert len(wallet.api.info()) == 53, "Info have incorrect data set"
