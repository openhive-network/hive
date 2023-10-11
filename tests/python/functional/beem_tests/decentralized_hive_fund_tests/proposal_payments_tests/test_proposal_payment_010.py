from __future__ import annotations

import datetime

import hive_utils
import test_tools as tt
from hive_local_tools.functional.python.beem import NodeClientMaker
from hive_local_tools.functional.python.beem.decentralized_hive_fund import CREATOR, TREASURY, test_utils


# 1. create few proposals - in this scenario proposals have different starting and ending dates
# 2. vote on them to show differences in asset distribution (depending on collected votes)
# 3. wait for proposal payment phase
# 4. verify (using account history and by checking regular account balance) that given accounts have been correctly paid
def test_proposal_payment_010(node_client: NodeClientMaker):
    accounts = [
        # place accounts here in the format: {'name' : name, 'private_key' : private-key, 'public_key' : public-key}
        {
            "name": "tester001",
            "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa",
            "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J",
        },
        {
            "name": "tester002",
            "private_key": "5KgfcV9bgEen3v9mxkoGw6Rhuf2giDRZTHZjzwisjkrpF4FUh3N",
            "public_key": "TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn",
        },
        {
            "name": "tester003",
            "private_key": "5Jz3fcrrgKMbL8ncpzTdQmdRVHdxMhi8qScoxSR3TnAFUcdyD5N",
            "public_key": "TST57wy5bXyJ4Z337Bo6RbinR6NyTRJxzond5dmGsP4gZ51yN6Zom",
        },
        {
            "name": "tester004",
            "private_key": "5KcmobLVMSAVzETrZxfEGG73Zvi5SKTgJuZXtNgU3az2VK3Krye",
            "public_key": "TST8dPte853xAuLMDV7PTVmiNMRwP6itMyvSmaht7J5tVczkDLa5K",
        },
    ]

    wif = tt.Account("initminer").private_key
    node_client = node_client(accounts=accounts)

    test_utils.create_accounts(node_client, CREATOR, accounts)
    test_utils.transfer_to_vesting(node_client, CREATOR, accounts, "300.000", "TESTS")

    tt.logger.info("Wait 30 days for full voting power")
    hive_utils.debug_quick_block_skip(node_client, wif, (30 * 24 * 3600 / 3))
    hive_utils.debug_generate_blocks(node_client.rpc.url, wif, 10)

    # transfer assets to accounts
    test_utils.transfer_assets_to_accounts(node_client, CREATOR, accounts, "400.000", "TESTS", wif)

    test_utils.transfer_assets_to_accounts(node_client, CREATOR, accounts, "400.000", "TBD", wif)

    tt.logger.info("Balances for accounts after initial transfer")
    test_utils.print_balance(node_client, accounts)
    # transfer assets to treasury
    test_utils.transfer_assets_to_treasury(node_client, CREATOR, TREASURY, "1000000.000", "TBD", wif)
    test_utils.print_balance(node_client, [{"name": TREASURY}])

    # create post for valid permlinks
    test_utils.create_posts(node_client, accounts, wif)

    now = node_client.get_dynamic_global_properties(False).get("time", None)
    if now is None:
        raise ValueError("Head time is None")
    now = test_utils.date_from_iso(now)

    proposal_data = [
        ["tester001", 1 + 0, 5, "24.000 TBD"],  # starts one day from now and lasts five days
        ["tester002", 1 + 0, 2, "24.000 TBD"],  # starts one day from now and lasts two days
        ["tester003", 1 + 2, 1, "24.000 TBD"],  # starts three days from now and lasts one day
        ["tester004", 1 + 4, 1, "24.000 TBD"],  # starts five days from now and lasts one day
    ]

    proposals = [
        # pace proposals here in the format: {'creator' : creator, 'receiver' : receiver, 'start_date' : start-date, 'end_date' : end_date}
    ]

    for pd in proposal_data:
        start_date, end_date = test_utils.get_start_and_end_date(now, pd[1], pd[2])
        proposal = {
            "creator": pd[0],
            "receiver": pd[0],
            "start_date": start_date,
            "end_date": end_date,
            "daily_pay": pd[3],
        }
        proposals.append(proposal)

    test_start_date = now + datetime.timedelta(days=1)
    test_start_date_iso = test_utils.date_to_iso(test_start_date)

    test_end_date = test_start_date + datetime.timedelta(days=6, hours=1)
    test_end_date_iso = test_utils.date_to_iso(test_end_date)

    test_utils.create_proposals(node_client, proposals, wif)

    # list proposals with inactive status, it shoud be list of pairs id:total_votes
    test_utils.list_proposals(node_client, test_start_date_iso, "inactive")

    # each account is voting on proposal
    test_utils.vote_proposals(node_client, accounts, wif)

    # list proposals with inactive status, it shoud be list of pairs id:total_votes
    votes = test_utils.list_proposals(node_client, test_start_date_iso, "inactive")
    for vote in votes:
        # should be 0 for all
        assert vote == 0, "All votes should be equal to 0"

    tt.logger.info("Balances for accounts after creating proposals")
    balances = test_utils.print_balance(node_client, accounts)
    for balance in balances:
        # should be 390.000 TBD for all
        assert balance == "390000", "All balances should be equal to 390.000 TBD"
    test_utils.print_balance(node_client, [{"name": TREASURY}])

    # move forward in time to see if proposals are paid
    # moving is made in 1h increments at a time, after each
    # increment balance is printed
    tt.logger.info(f"Moving to date: {test_start_date_iso}")
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, test_start_date_iso, False)
    current_date = test_start_date
    while current_date < test_end_date:
        current_date = current_date + datetime.timedelta(hours=1)
        current_date_iso = test_utils.date_to_iso(current_date)

        tt.logger.info(f"Moving to date: {current_date_iso}")
        hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, current_date_iso, False)

        tt.logger.info(f"Balances for accounts at time: {current_date_iso}")
        test_utils.print_balance(node_client, accounts)
        test_utils.print_balance(node_client, [{"name": TREASURY}])

        votes = test_utils.list_proposals(node_client, test_start_date_iso, "active")
        votes = test_utils.list_proposals(node_client, test_start_date_iso, "expired")
        votes = test_utils.list_proposals(node_client, test_start_date_iso, "all")

    # move additional hour to ensure that all proposals ended
    tt.logger.info(f"Moving to date: {test_end_date_iso}")
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, test_end_date_iso, False)
    tt.logger.info(f"Balances for accounts at time: {test_end_date_iso}")
    balances = test_utils.print_balance(node_client, accounts)

    # 'HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD' == 5 days, so in 5th day any proposal is not paid at all, therefore:
    #     An account 'tester001' got only 486.000 TBD
    #     An account 'tester004' didn't get any reward
    test_balances = [
        "486000",
        "438000",
        "414000",
        "390000",
    ]

    for idx in range(len(test_balances)):
        assert balances[idx] == test_balances[idx], f"Balances dont match {balances[idx]} != {test_balances[idx]}"

    test_utils.print_balance(node_client, [{"name": TREASURY}])
