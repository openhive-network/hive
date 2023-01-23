import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_OPEN_RECURRENT_TRANSFERS


@run_for("testnet")
def test_massive_recurrent_transfer_one_to_more_than_255_accounts(node):
    amount_of_receiver_accounts = MAX_OPEN_RECURRENT_TRANSFERS
    account_that_sends_all_recurrent_transfers = "sender"
    excess_account_name = "excess-account"

    wallet = tt.Wallet(attach_to=node)

    wallet.create_account(account_that_sends_all_recurrent_transfers,
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    wallet.create_account(excess_account_name)

    receiver_accounts = wallet.create_accounts(amount_of_receiver_accounts)

    # Order a 255 recurrent transfers form one account
    with wallet.in_single_transaction():
        for account in receiver_accounts:
            wallet.api.recurrent_transfer(account_that_sends_all_recurrent_transfers,
                                          account.name,
                                          tt.Asset.Test(0.001),
                                          f"recurrent transfer to {account.name}",
                                          24,
                                          2)

    # Validate that "sender" order 255 recurrent transfers
    assert len(wallet.api.find_recurrent_transfers(account_that_sends_all_recurrent_transfers)) == \
           amount_of_receiver_accounts

    # Validate there is exception after order 256th recurrent transfer from the same sender
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.recurrent_transfer(account_that_sends_all_recurrent_transfers,
                                      excess_account_name,
                                      tt.Asset.Test(0.001),
                                      f"recurrent transfer to {excess_account_name}",
                                      24,
                                      2)
