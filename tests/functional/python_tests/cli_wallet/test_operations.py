import test_tools as tt
from hive_local_tools.functional.python.cli_wallet import funded_account_info


def test_delayed_voting(wallet: tt.Wallet, funded_account: funded_account_info, creator: tt.Account):
    account = funded_account.account

    def count_votes():
        dv = [int(item["val"]) for item in wallet.api.get_account(account_name=account.name)["delayed_votes"]]
        return sum(dv)

    votes_before = count_votes()
    wallet.api.transfer_to_vesting(from_=creator.name, to=account.name, amount=tt.Asset.Test(1))
    votes_after = count_votes()

    assert votes_after > votes_before


def test_get_open_orders(wallet: tt.Wallet, funded_account: funded_account_info):
    user = funded_account.account
    AMOUNT_TO_SELL_1 = tt.Asset.Test(10)
    MIN_TO_RECEIVE_1 = tt.Asset.Tbd(1000)

    AMOUNT_TO_SELL_2 = tt.Asset.Tbd(10)
    MIN_TO_RECEIVE_2 = tt.Asset.Test(1000)

    result_before = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_before) == 0

    tt.logger.info(f"testing buy order: {AMOUNT_TO_SELL_1} for {MIN_TO_RECEIVE_1} created by user {user.name}")
    wallet.api.create_order(
        owner=user.name,
        order_id=1,
        amount_to_sell=AMOUNT_TO_SELL_1,
        min_to_receive=MIN_TO_RECEIVE_1,
        fill_or_kill=False,
        expiration=9999,
    )
    result_sell = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_sell) == 1
    assert result_sell[0]["orderid"] == 1
    assert result_sell[0]["seller"] == user.name
    assert result_sell[0]["for_sale"] == AMOUNT_TO_SELL_1.amount
    assert result_sell[0]["sell_price"]["base"] == AMOUNT_TO_SELL_1
    assert result_sell[0]["sell_price"]["quote"] == MIN_TO_RECEIVE_1

    tt.logger.info(f"testing buy order: {AMOUNT_TO_SELL_2} for {MIN_TO_RECEIVE_2} created by user {user.name}")
    wallet.api.create_order(
        owner=user.name,
        order_id=2,
        amount_to_sell=AMOUNT_TO_SELL_2,
        min_to_receive=MIN_TO_RECEIVE_2,
        fill_or_kill=False,
        expiration=9999,
    )
    result_buy = wallet.api.get_open_orders(accountname=user.name)
    assert len(result_buy) == 2
    assert result_buy[1]["orderid"] == 2
    assert result_buy[1]["seller"] == user.name
    assert result_buy[1]["for_sale"] == AMOUNT_TO_SELL_2.amount
    assert result_buy[1]["sell_price"]["base"] == AMOUNT_TO_SELL_2
    assert result_buy[1]["sell_price"]["quote"] == MIN_TO_RECEIVE_2


def test_create_recurent_transfer(wallet: tt.Wallet, funded_account: funded_account_info, creator: tt.Account):
    receiver = funded_account.account
    MEMO = "This is a memo"
    AMOUNT = tt.Asset.Tbd(10)
    RECURRENCE = 24
    EXECUTIONS = 6

    recurrent_transfers_before_count = len(wallet.api.find_recurrent_transfers(from_=creator.name))
    tt.logger.info(f"recurrent_transfers: {recurrent_transfers_before_count}")

    wallet.api.recurrent_transfer(
        from_=creator.name, to=receiver.name, amount=AMOUNT, memo=MEMO, recurrence=RECURRENCE, executions=EXECUTIONS
    )

    recurrent_transfers = wallet.api.find_recurrent_transfers(from_=creator.name)
    recurrent_transfers_after_count = len(recurrent_transfers)
    recurrent_transfer = recurrent_transfers[recurrent_transfers_after_count - 1]
    tt.logger.info(f"recurrent_transfers_after_countL {recurrent_transfers_after_count}")

    assert recurrent_transfers_before_count + 1 == recurrent_transfers_after_count
    assert recurrent_transfer["from"] == creator.name
    assert recurrent_transfer["to"] == receiver.name
    assert recurrent_transfer["amount"] == AMOUNT
    assert recurrent_transfer["memo"] == MEMO
    assert recurrent_transfer["recurrence"] == RECURRENCE
    assert recurrent_transfer["consecutive_failures"] == 0
    assert recurrent_transfer["remaining_executions"] == EXECUTIONS - 1
