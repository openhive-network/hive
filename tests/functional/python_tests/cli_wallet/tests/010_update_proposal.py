#!/usr/bin/python3

import time

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

def proposal_exists( block_number, end_date ):
  delay = 66
  print("Waiting for accept reversible blocks")
  time.sleep(delay)
  print("End of waiting")

  output_ops = wallet.get_ops_in_block( block_number, False )
  print( output_ops )

  if 'result' in output_ops:
    _result = output_ops['result']
    if len(_result) >= 1:
      if 'op' in _result[0]:
        ops = _result[0]['op']
        if len(ops) == 2:
          _type = ops[0]
          _value = ops[1]
          if 'extensions' in _value:
            _extensions = _value['extensions']
            if len(_extensions) == 1:
              __extensions = _extensions[0]
              if len(__extensions) == 2:
                _extension_type = __extensions[0]
                _extension_value = __extensions[1]
                if 'end_date' in _extension_value:
                  _end_date = _extension_value['end_date']
                if _extension_type == 1 and _end_date == end_date:
                  return True
  return False

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, receiver = make_user_for_tests(wallet)
            wallet.post_comment(creator, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
            wallet.create_proposal(creator, receiver, "2029-06-02T00:00:00", "2029-08-01T00:00:00", "10.000 TBD", "this is subject", "lorem", "true")
            proposals = find_creator_proposals(creator, wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))

            proposal_id = -1
            for proposal in proposals:
                if proposal['receiver'] == receiver:
                    proposal_id = proposal['proposal_id']
                    break

            assert proposal_id != -1, "Proposal just created was not found"

            log.info("testing updating the proposal with the end date")
            output = wallet.update_proposal(proposal_id, creator, "9.000 TBD", "this is an updated subject", "lorem", "2029-07-25T00:00:00", "true")

            block = output['result']['ref_block_num'] + 1
            print( "block: {}".format(block) );
            check_proposal = proposal_exists( block, "2029-07-25T00:00:00" )
            assert( check_proposal )

            proposal = wallet.find_proposals([proposal_id])['result'][0]

            assert(proposal['daily_pay'] == "9.000 TBD")
            assert(proposal['subject'] == "this is an updated subject")
            assert(proposal['permlink'] == "lorem")
            assert(proposal['end_date'] == "2029-07-25T00:00:00")

            log.info("testing updating the proposal without the end date")
            test = wallet.update_proposal(proposal_id, creator, "8.000 TBD", "this is an updated subject again", "lorem", None, "true")
            proposal = wallet.find_proposals([proposal_id])['result'][0]

            assert(proposal['daily_pay'] == "8.000 TBD")
            assert(proposal['subject'] == "this is an updated subject again")
            assert(proposal['permlink'] == "lorem")
            assert(proposal['end_date'] == "2029-07-25T00:00:00")
