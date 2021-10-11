from test_tools import Wallet

from ..cli_wallet_extended_tests.utilities import result_of


def test_info_function(wallet: Wallet):
    assert len(result_of(wallet.api.info)) == 54, "Info have incorrect data set"
