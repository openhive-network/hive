import datetime

import test_tools as tt
import hive_utils
from hive_local_tools.functional.python.beem import NodeClientMaker
from hive_local_tools.functional.python.beem.decentralized_hive_fund import CREATOR, TREASURY, test_utils


# Greedy baby scenario

# 0. In this scenario we have one proposal with huge daily pay and couple with low daily pay
#    all proposals have the same number of votes, greedy proposal is last
# 1. create few proposals - in this scenario proposals have different starting and ending dates
# 2. vote on them to show differences in asset distribution (depending on collected votes)
# 3. wait for proposal payment phase
# 4. verify (using account history and by checking regular account balance) that given accounts have been correctly paid
# Expected result: all got paid.
def test_proposal_payment_006(node_client: NodeClientMaker):
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

    account_names = [v["name"] for v in accounts]

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
        ["tester001", 1 + 0, 1, 24.000],  # starts 1 day from now and lasts 1 day
        ["tester002", 1 + 1, 1, 24.000],  # starts 2 days from now and lasts 1 day
        ["tester003", 1 + 2, 1, 24.000],  # starts 3 days from now and lasts 1 day
        ["tester004", 1 + 0, 3, 240000.000],  # starts one day from now and lasts 3 days
    ]
    proposals_daily_pay = [(prop[0], prop[3]) for prop in proposal_data]
    proposals_datetime_ranges = {}

    proposals = [
        # pace proposals here in the format: {'creator' : creator, 'receiver' : receiver, 'start_date' : start-date, 'end_date' : end_date}
    ]

    start = None
    for pd in proposal_data:
        start_date, end_date = test_utils.get_start_and_end_date(now, pd[1], pd[2])
        if start is None:
            start = test_utils.date_from_iso(start_date)
        proposal = {
            "creator": pd[0],
            "receiver": pd[0],
            "start_date": start_date,
            "end_date": end_date,
            "daily_pay": f"{pd[3] :.3f} TBD",
        }
        proposals.append(proposal)
        proposals_datetime_ranges[pd[0]] = {
            "start": test_utils.date_from_iso(start_date),
            "end": test_utils.date_from_iso(end_date),
        }

    test_utils.create_proposals(node_client, proposals, wif)

    # each account is voting on proposal
    test_utils.vote_proposals(node_client, accounts, wif)

    propos = node_client.get_dynamic_global_properties(False)
    period = test_utils.date_from_iso(propos["next_maintenance_time"])

    while period + datetime.timedelta(hours=1) < start:
        period = period + datetime.timedelta(hours=1)

    pre_test_start_date = period
    test_start_date = pre_test_start_date
    pre_test_start_date = test_start_date - datetime.timedelta(seconds=2)
    test_start_date_iso = test_utils.date_to_iso(test_start_date)
    pre_test_start_date_iso = test_utils.date_to_iso(pre_test_start_date)

    test_end_date = test_start_date + datetime.timedelta(days=3)
    test_end_date_iso = test_utils.date_to_iso(test_end_date)

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
    # increment balance is printed and checked
    tt.logger.info("Moving to date: {}".format(test_start_date_iso))
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, pre_test_start_date_iso, False)
    previous_balances = dict(zip(account_names, test_utils.print_balance(node_client, accounts)))
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, test_start_date_iso, False)
    current_date = test_start_date

    while current_date < test_end_date:
        current_date = current_date + datetime.timedelta(hours=1)
        current_date_iso = test_utils.date_to_iso(current_date)

        tt.logger.info("Moving to date: {}".format(current_date_iso))
        budget = test_utils.calculate_propsal_budget(node_client, TREASURY, wif)
        hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, current_date_iso, False)

        tt.logger.info("Balances for accounts at time: {}".format(current_date_iso))
        accnts = dict(zip(account_names, test_utils.print_balance(node_client, accounts)))
        expected_results = dict()
        for acc, payout in proposals_daily_pay:
            date_ranges = proposals_datetime_ranges[acc]
            if current_date >= date_ranges["start"] and current_date < date_ranges["end"]:
                expected_results[acc] = payout
        expected_results = test_utils.calculate_expected_hourly_payout(expected_results, budget)

        for acc, ret in accnts.items():
            if acc in expected_results.keys():
                # because of rounding mechanism
                assert (
                    abs((int(previous_balances[acc]) + expected_results[acc]) - int(ret)) < 2
                ), f"too big missmatch, prev: {previous_balances[acc]}, budget: {budget}, now: {ret}, expected: {expected_results[acc]}, account: {acc}"

        previous_balances = accnts

    # move additional hour to ensure that all proposals ended
    tt.logger.info("Moving to date: {}".format(test_end_date_iso))
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, current_date_iso, False)
    tt.logger.info("Balances for accounts at time: {}".format(test_end_date_iso))
    test_utils.print_balance(node_client, accounts)