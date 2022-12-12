import datetime

import dateutil.parser

from .. import test_utils
from ..... import hive_utils
import test_tools as tt
from hive_local_tools.functional.python.beem import NodeClientMaker
from hive_local_tools.functional.python.beem.decentralized_hive_fund import create_proposals, CREATOR, TREASURY

# 1. create few proposals - in this scenario all proposals have the same start and end date
# 2. vote on them to show differences in asset distribution (depending on collected votes)
# 3. wait for proposal payment phase
# 4. verify (using account history and by checking regular account balance) that given accounts have been correctly paid
def test_proposal_payment_001(node_client: NodeClientMaker):
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
        {
            "name": "tester005",
            "private_key": "5Hy4vEeYmBDvmXipe5JAFPhNwCnx7NfsfyiktBTBURn9Qt1ihcA",
            "public_key": "TST7CP7FFjvG55AUeH8riYbfD8NxTTtFH32ekQV4YFXmV6gU8uAg3",
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
    now = dateutil.parser.parse(now)

    start_date = now + datetime.timedelta(days=1)
    end_date = start_date + datetime.timedelta(days=2)

    end_date_blocks = start_date + datetime.timedelta(days=2, hours=1)

    start_date_str = start_date.replace(microsecond=0).isoformat()
    end_date_str = end_date.replace(microsecond=0).isoformat()

    end_date_blocks_str = end_date_blocks.replace(microsecond=0).isoformat()

    # create proposals - each account creates one proposal
    create_proposals(node_client, accounts, start_date_str, end_date_str, wif)

    # list proposals with inactive status, it shoud be list of pairs id:total_votes
    test_utils.list_proposals(node_client, start_date_str, "inactive")

    # each account is voting on proposal
    test_utils.vote_proposals(node_client, accounts, wif)

    # list proposals with inactive status, it shoud be list of pairs id:total_votes
    votes = test_utils.list_proposals(node_client, start_date_str, "inactive")
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
    tt.logger.info("Moving to date: {}".format(start_date_str))
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, start_date_str, False)
    current_date = start_date
    while current_date < end_date:
        current_date = current_date + datetime.timedelta(hours=1)
        current_date_str = current_date.replace(microsecond=0).isoformat()
        tt.logger.info("Moving to date: {}".format(current_date_str))
        hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, current_date_str, False)

        tt.logger.info("Balances for accounts at time: {}".format(current_date_str))
        test_utils.print_balance(node_client, accounts)
        test_utils.print_balance(node_client, [{"name": TREASURY}])
        votes = test_utils.list_proposals(node_client, start_date_str, "active")
        for vote in votes:
            # should be > 0 for all
            assert vote > 0, "All votes counts shoud be greater than 0"

    # move additional hour to ensure that all proposals ended
    tt.logger.info("Moving to date: {}".format(end_date_blocks_str))
    hive_utils.common.debug_generate_blocks_until(node_client.rpc.url, wif, end_date_blocks_str, False)
    tt.logger.info("Balances for accounts at time: {}".format(end_date_blocks_str))
    balances = test_utils.print_balance(node_client, accounts)
    for balance in balances:
        assert balance == "438000", "All balances should be equal 438000"
    test_utils.print_balance(node_client, [{"name": TREASURY}])
    votes = test_utils.list_proposals(node_client, start_date_str, "expired")
    for vote in votes:
        # should be > 0 for all
        assert vote > 0, "All votes counts shoud be greater than 0"
