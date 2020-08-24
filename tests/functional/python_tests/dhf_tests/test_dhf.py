import pytest
import logging
import hive_utils
import test_utils

logger = logging.getLogger()

try:
    from beem import Hive
except Exception as ex:
    logger.exception("beem library is not installed.")
    raise ex

@pytest.fixture(scope="module")
def subject():
  from uuid import uuid4
  return str(uuid4())


def test_create_proposal(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: create_proposal")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    creator_account =  pytestconfig.getoption('--creator', None)
    assert creator_account is not None, '--creator option not set'
    receiver_account =  pytestconfig.getoption('--receiver', None)
    assert receiver_account is not None, '--receiver option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    
    import datetime
    now = datetime.datetime.now()

    start_date, end_date = test_utils.get_start_and_end_date(now, 10, 2)

    from beem.account import Account
    try:
        creator = Account(creator_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(creator_account, ex))
        raise ex
    
    try:
        receiver = Account(receiver_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(receiver_account, ex))
        raise ex

    ret = s.post("Hivepy proposal title", "Hivepy proposal body", creator["name"], permlink = "hivepy-proposal-title", tags = "proposals")
    from beembase.operations import Create_proposal
    op = Create_proposal(
        **{
            "creator" : creator["name"], 
            "receiver" : receiver["name"], 
            "start_date" : start_date, 
            "end_date" : end_date,
            "daily_pay" : "16.000 TBD",
            "subject" : subject,
            "permlink" : "hivepy-proposal-title"
        }
    )
    ret = None
    try:
        ret = s.finalizeOp(op, creator["name"], "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex

    hive_utils.common.wait_n_blocks(node, 3)

    assert ret["operations"][0][1]["creator"] == creator["name"]
    assert ret["operations"][0][1]["receiver"] == receiver["name"]
    assert ret["operations"][0][1]["start_date"] == start_date
    assert ret["operations"][0][1]["end_date"] == end_date
    assert ret["operations"][0][1]["daily_pay"] == "16.000 TBD"
    assert ret["operations"][0][1]["subject"] == subject
    assert ret["operations"][0][1]["permlink"] == "hivepy-proposal-title"

def test_list_proposals(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: list_proposals")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    # list inactive proposals, our proposal shoud be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is not None
    
    # list active proposals, our proposal shouldnt be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "active")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is None

    # list all proposals, our proposal should be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "all")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is not None

def test_find_proposals(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: find_proposals")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is not None
    proposal_id = int(found["proposal_id"])

    ret = s.rpc.find_proposals([proposal_id])
    assert ret[0]["subject"] == found["subject"]

def test_vote_proposal(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: vote_proposal")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is not None
    proposal_id = int(found["proposal_id"])

    # now lets vote
    from beembase.operations import Update_proposal_votes
    op = Update_proposal_votes(
        **{
            "voter" : account,
            "proposal_ids" : [proposal_id],
            "approve" : True
        }
    )

    ret = None
    try:
        ret = s.finalizeOp(op, account, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node, 3)

    assert ret["operations"][0][1]["voter"] == account
    assert ret["operations"][0][1]["proposal_ids"][0] == proposal_id
    assert ret["operations"][0][1]["approve"] == True

def test_list_voter_proposals(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: list_voter_proposals")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    voter_proposals = s.rpc.list_proposal_votes([account], 1000, "by_voter_proposal", "ascending", "inactive")

    found = None
    for proposals in voter_proposals:
        if proposals["proposal"]["subject"] == subject:
            found = proposals
    
    assert found is not None

def test_update_proposal(hive_node_provider, pytestconfig):
    logger.info("Testing: update_proposal")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    creator =  pytestconfig.getoption('--creator', None)
    assert creator is not None, '--creator option not set'
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    proposals = s.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    from beembase.operations import Update_proposal
    subject = "Some new proposal subject"
    op = Update_proposal(**{
        'proposal_id' : proposals[0]["proposal_id"],
        'creator' : proposals[0]["creator"],
        'daily_pay' : "16.000 TBD",
        'subject' : subject,
        'permlink': proposals[0]["permlink"]
    })
    try:
        s.finalizeOp(op, creator, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node, 3)

    proposals = s.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    assert proposals[0]["subject"] == subject, "Subjects dont match"

def test_remove_proposal(hive_node_provider, pytestconfig):
    logger.info("Testing: remove_proposal")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    subject = "Some new proposal subject"
    s = Hive(node = [node], no_broadcast = False, keys = [wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is not None, "Not found"
    proposal_id = int(found["proposal_id"])

    # remove proposal
    from beembase.operations import Remove_proposal
    op = Remove_proposal(
        **{
            'proposal_owner' : account,
            'proposal_ids' : [proposal_id]
        }
    )

    try:
        s.finalizeOp(op, account, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node, 3)

    # try to find our special proposal
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal
    
    assert found is None, "Not found"

def test_iterate_results_test(hive_node_provider, pytestconfig, subject):
    logger.info("Testing: test_iterate_results_test")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    creator_account =  pytestconfig.getoption('--creator', None)
    assert creator_account is not None, '--creator option not set'
    receiver_account = pytestconfig.getoption('--receiver', None)
    assert receiver_account is not None, '--receiver option not set'
    remove = pytestconfig.getoption('--no-erase-proposal', True)
    # test for iterate prosals
    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    #   in real life scenatio pagination scheme with limit set to value lower than "k" will be showing
    #   the same results and will hang
    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    s = Hive(node = [node], no_broadcast = False, keys = [wif])

    from beem.account import Account
    try:
        creator = Account(creator_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(creator_account, ex))
        raise ex
    
    try:
        receiver = Account(receiver_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(receiver_account, ex))
        raise ex

    import datetime
    now = datetime.datetime.now()

    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # here we have 5 proposals with the same start date
    start_end_pairs = [
        [1,1],
        [2,2],
        [4,3],
        [5,4],
        [5,5],
        [5,6],
        [5,7],
        [5,8],
        [6,9]
    ]

    from beembase.operations import Create_proposal
    for start_end_pair in start_end_pairs:
        start_date, end_date = test_utils.get_start_and_end_date(now, start_end_pair[0], start_end_pair[1])
        op = Create_proposal(
            **{
                'creator' : creator["name"], 
                'receiver' : receiver["name"], 
                'start_date' : start_date, 
                'end_date' :end_date,
                'daily_pay' : "16.000 TBD",
                'subject' : subject,
                'permlink' : "hivepy-proposal-title"
            }
        )
        try:
            s.finalizeOp(op, creator["name"], "active")
        except Exception as ex:
            logger.exception("Exception: {}".format(ex))
            raise ex
    hive_utils.common.wait_n_blocks(node, 5)

    start_date = test_utils.date_to_iso(now + datetime.timedelta(days = 5))

    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, "Expected {} elements got {}".format(3, len(proposals))
    ids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(start_date, proposals[-1]["start_date"])
        ids.append(proposal["proposal_id"])
    assert len(ids) == 3, "Expected {} elements got {}".format(3, len(ids))

    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, "Expected {} elements got {}".format(3, len(proposals))
    oids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(start_date, proposals[-1]["start_date"])
        oids.append(proposal["proposal_id"])
    assert len(oids) == 3, "Expected {} elements got {}".format(3, len(oids))

    # the same set of results check
    for id in ids:
        assert id in oids, "Id not found in expected results array {}".format(id)

    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all", oids[-1])

    start_date, end_date = test_utils.get_start_and_end_date(now, 5, 4)

    assert proposals[-1]["start_date"] == start_date, "Expected start_date do not match {} != {}".format(start_date, proposals[-1]["start_date"])
    assert proposals[-1]["end_date"] == end_date, "Expected end_date do not match {} != {}".format(end_date, proposals[-1]["end_date"])

    # remove all created proposals
    from beembase.operations import Remove_proposal
    if not remove:
        start_date = test_utils.date_to_iso(now + datetime.timedelta(days = 6))
        for _ in range(0, 2):
            proposals = s.rpc.list_proposals([start_date], 5, "by_start_date", "descending", "all")
            ids = []
            for proposal in proposals:
                ids.append(int(proposal['proposal_id']))
            ids.sort()
            if ids:
                op = Remove_proposal(
                    **{
                        "proposal_owner" : creator["name"],
                        "proposal_ids" : ids
                    }
                )
                try:
                    s.finalizeOp(op, creator["name"], "active")
                except Exception as ex:
                    logger.exception("Exception: {}".format(ex))
                    raise ex
                hive_utils.common.wait_n_blocks(node, 3)


