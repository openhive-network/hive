from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt

if TYPE_CHECKING:
    from hive_local_tools.functional.python.cli_wallet import FundedAccountInfo


def test_delayed_voting(wallet: tt.OldWallet, funded_account: FundedAccountInfo, creator: tt.Account) -> None:
    account = funded_account.account

    def count_votes() -> int:
        dv = [int(item["val"]) for item in wallet.api.get_account(account_name=account.name)["delayed_votes"]]
        return sum(dv)

    votes_before = count_votes()
    wallet.api.transfer_to_vesting(from_=creator.name, to=account.name, amount=tt.Asset.Test(1))
    votes_after = count_votes()

    assert votes_after > votes_before


def test_get_open_orders(wallet: tt.OldWallet, funded_account: FundedAccountInfo) -> None:
    user = funded_account.account
    amount_to_sell_1 = tt.Asset.Test(10)
    min_to_receive_1 = tt.Asset.Tbd(1000)

    amount_to_sell_2 = tt.Asset.Tbd(10)
    min_to_receive_2 = tt.Asset.Test(1000)

    result_before = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_before) == 0

    tt.logger.info(f"testing buy order: {amount_to_sell_1} for {min_to_receive_1} created by user {user.name}")
    wallet.api.create_order(
        owner=user.name,
        order_id=1,
        amount_to_sell=amount_to_sell_1,
        min_to_receive=min_to_receive_1,
        fill_or_kill=False,
        expiration=9999,
    )
    result_sell = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_sell) == 1
    assert result_sell[0]["orderid"] == 1
    assert result_sell[0]["seller"] == user.name
    assert result_sell[0]["for_sale"] == int(amount_to_sell_1.amount)
    assert tt.Asset.from_legacy(result_sell[0]["sell_price"]["base"]) == amount_to_sell_1
    assert tt.Asset.from_legacy(result_sell[0]["sell_price"]["quote"]) == min_to_receive_1

    tt.logger.info(f"testing buy order: {amount_to_sell_2} for {min_to_receive_2} created by user {user.name}")
    wallet.api.create_order(
        owner=user.name,
        order_id=2,
        amount_to_sell=amount_to_sell_2,
        min_to_receive=min_to_receive_2,
        fill_or_kill=False,
        expiration=9999,
    )
    result_buy = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_buy) == 2
    assert result_buy[1]["orderid"] == 2
    assert result_buy[1]["seller"] == user.name
    assert result_buy[1]["for_sale"] == int(amount_to_sell_2.amount)
    assert tt.Asset.from_legacy(result_buy[1]["sell_price"]["base"]) == amount_to_sell_2
    assert tt.Asset.from_legacy(result_buy[1]["sell_price"]["quote"]) == min_to_receive_2


def test_create_recurent_transfer(wallet: tt.OldWallet, funded_account: FundedAccountInfo, creator: tt.Account) -> None:
    receiver = funded_account.account
    memo = "This is a memo"
    amount = tt.Asset.Tbd(10)
    recurrence = 24
    executions = 6
    pair_id = 0

    recurrent_transfers_before_count = len(wallet.api.find_recurrent_transfers(from_=creator.name))
    tt.logger.info(f"recurrent_transfers: {recurrent_transfers_before_count}")

    wallet.api.recurrent_transfer(
        from_=creator.name,
        to=receiver.name,
        amount=amount,
        memo=memo,
        recurrence=recurrence,
        executions=executions,
    )

    recurrent_transfers = wallet.api.find_recurrent_transfers(from_=creator.name)
    recurrent_transfers_after_count = len(recurrent_transfers)
    recurrent_transfer = recurrent_transfers[recurrent_transfers_after_count - 1]
    tt.logger.info(f"recurrent_transfers_after_countL {recurrent_transfers_after_count}")

    assert recurrent_transfers_before_count + 1 == recurrent_transfers_after_count
    assert recurrent_transfer["from"] == creator.name
    assert recurrent_transfer["to"] == receiver.name
    assert tt.Asset.from_legacy(recurrent_transfer["amount"]) == amount
    assert recurrent_transfer["memo"] == memo
    assert recurrent_transfer["recurrence"] == recurrence
    assert recurrent_transfer["consecutive_failures"] == 0
    assert recurrent_transfer["remaining_executions"] == executions - 1
    assert recurrent_transfer["pair_id"] == pair_id
