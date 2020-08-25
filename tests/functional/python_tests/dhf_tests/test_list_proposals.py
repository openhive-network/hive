import pytest
import logging

import hive_utils

logger = logging.getLogger()

try:
  from beem import Hive
except Exception as ex:
  logger.exception("beem library is not installed.")
  raise ex

START_END_SUBJECTS = [
  [1,3,"Subject001"],
  [2,3,"Subject002"],
  [3,3,"Subject003"],
  [4,3,"Subject004"],
  [5,3,"Subject005"],
  [6,3,"Subject006"],
  [7,3,"Subject007"],
  [8,3,"Subject008"],
  [9,3,"Subject009"]
]


def test_create_proposals(hive_node_provider, pytestconfig):
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

  node_client = Hive(node = [node], no_broadcast = False, keys = [wif])

  import datetime
  now = datetime.datetime.now()

  start_date, end_date = hive_utils.common.get_isostr_start_end_date(now, 10, 2)

  from beem.account import Account
  try:
    creator = Account(creator_account, hive_instance=node_client)
  except Exception as ex:
    logger.error("Account: {} not found. {}".format(creator_account, ex))
    raise ex

  try:
    receiver = Account(receiver_account, hive_instance=node_client)
  except Exception as ex:
    logger.error("Account: {} not found. {}".format(receiver_account, ex))
    raise ex

  logger.info("Creating initial post...")
  node_client.post("Hivepy proposal title", "Hivepy proposal body", creator["name"], permlink = "hivepy-proposal-title", tags = "proposals")

  logger.info("Creating proposals...")
  from beembase.operations import Create_proposal
  for start_end_subject in START_END_SUBJECTS:
    start_date, end_date = hive_utils.common.get_isostr_start_end_date(now, start_end_subject[0], start_end_subject[1])
    op = Create_proposal(
      **{
        'creator' : creator["name"],
        'receiver' : receiver["name"],
        'start_date' : start_date,
        'end_date' : end_date,
        'daily_pay' : "16.000 TBD",
        'subject' : start_end_subject[2],
        'permlink' : "hivepy-proposal-title"
      }
    )
    try:
      node_client.finalizeOp(op, creator["name"], "active")
    except Exception as ex:
      logger.exception("Exception: {}".format(ex))
      raise ex

    hive_utils.common.wait_n_blocks(node_client.rpc.url, 1)
  hive_utils.common.wait_n_blocks(node_client.rpc.url, 2)

def test_list_proposals(hive_node_provider, pytestconfig):
    logger.info("Testing direction ascending with start field given")
    assert hive_node_provider is not None, "Node is None"
    assert hive_node_provider.is_running(), "Node is not running"
    node = pytestconfig.getoption('--node-url', None)
    assert node is not None, '--node-url option not set'
    wif = pytestconfig.getoption('--wif', None)
    assert wif is not None, '--wif option not set'
    account =  pytestconfig.getoption('--creator', None)
    assert account is not None, '--creator option not set'
    creator =  pytestconfig.getoption('--creator', None)
    assert creator is not None, '--creator option not set'
    
    node_client = Hive(node = [node], no_broadcast = False, keys = [wif])

    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(START_END_SUBJECTS), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert proposals[0]['subject'] == START_END_SUBJECTS[0][2], "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[0][2])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    logger.info("Testing direction by_creator ascending with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(START_END_SUBJECTS), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert proposals[0]['subject'] == START_END_SUBJECTS[0][2], "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[0][2])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    id_first = proposals[0]['id']
    id_last  = proposals[-1]['id']

    logger.info("Testing direction descending with start field given")
    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "descending", "all")
    # we should get len(START_END_SUBJECTS) proposals with first proposal with subject Subject009
    # and last with subject Subject001
    assert len(proposals) == len(START_END_SUBJECTS), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert proposals[0]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[-1][2])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[0][2], "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[0][2])

    logger.info("Testing direction descending bu_creator with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "descending", "all")
    assert len(proposals) == len(START_END_SUBJECTS), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert proposals[0]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[-1][2])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[0][2], "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[0][2])

    # if all pass we can proceed with other tests
    # first we will test empty start string in defferent directions
    logger.info("Testing empty start string and ascending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert proposals[0]['subject'] == START_END_SUBJECTS[0][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[0][2])

    logger.info("Testing by_start_date no start string and ascending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert proposals[0]['subject'] == START_END_SUBJECTS[0][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[0]['subject'], START_END_SUBJECTS[0][2])
    
    # now we will test empty start string in descending
    logger.info("Testing empty start string and descending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "descending", "all")
    assert proposals[0]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    # now we will test no start parameter in descending
    logger.info("Testing by_start_data with no start and descending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "descending", "all")
    assert proposals[0]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    # now we will test empty start string with ascending order and last_id set

    logger.info("Testing empty start string and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    logger.info("Testing no start parameter and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[0][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[0][2])

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing no start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[0][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[0][2])

    # now we will test empty start string with ascending order and last_id set
    logger.info("Testing not empty start string and ascenging direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[-1][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[-1][2])

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing not empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", 5)
    assert proposals[0]['id'] == 5, "First proposal should have id == 5, has {}".format(proposals[0]['id'])
    assert proposals[-1]['subject'] == START_END_SUBJECTS[0][2], "Subject of the proposal does not match with assumed proposal subject {}!={}".format(proposals[-1]['subject'], START_END_SUBJECTS[0][2])

    logger.info("Testing not empty start string and ascending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_last)
    assert len(proposals) == 1
    assert proposals[0]['id'] == id_last

    logger.info("Testing not empty start string and descending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_first)
    assert len(proposals) == 1
    assert proposals[0]['id'] == id_first

    logger.info("Testing not empty start string and ascending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_first)
    assert len(proposals) == len(START_END_SUBJECTS)

    logger.info("Testing not empty start string and descending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_last)
    assert len(proposals) == len(START_END_SUBJECTS)
