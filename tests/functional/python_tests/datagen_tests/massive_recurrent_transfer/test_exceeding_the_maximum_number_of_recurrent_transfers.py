import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_OPEN_RECURRENT_TRANSFERS


@run_for("testnet")
def test_try_to_send_more_than_the_maximum_limit_of_recurrent_transfers_from_one_account(node):
    amount_of_receiver_accounts = MAX_OPEN_RECURRENT_TRANSFERS
    account_that_sends_all_recurrent_transfers = "sender"
    account_that_exceed_transfers_limit = "additional-acc"

    wallet = tt.Wallet(attach_to=node)

    wallet.create_account(account_that_sends_all_recurrent_transfers,
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    wallet.create_account(account_that_exceed_transfers_limit)

    receiver_accounts = wallet.create_accounts(amount_of_receiver_accounts)

    # Send 255 recurrent transfers from one account
    with wallet.in_single_transaction():
        for account in receiver_accounts:
            wallet.api.recurrent_transfer(account_that_sends_all_recurrent_transfers,
                                          account.name,
                                          tt.Asset.Test(0.001),
                                          f"recurrent transfer to {account.name}",
                                          24,
                                          2)

    # Validate that "sender" sends 255 recurrent transfers
    assert len(wallet.api.find_recurrent_transfers(account_that_sends_all_recurrent_transfers)) == \
           amount_of_receiver_accounts

    # Validate there is exception after sending 256th recurrent transfer from the same sender
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.recurrent_transfer(account_that_sends_all_recurrent_transfers,
                                      account_that_exceed_transfers_limit,
                                      tt.Asset.Test(0.001),
                                      f"recurrent transfer to {account_that_exceed_transfers_limit}",
                                      24,
                                      2)
