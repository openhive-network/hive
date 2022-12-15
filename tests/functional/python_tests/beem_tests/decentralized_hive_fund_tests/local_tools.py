from typing import Final

from beembase.operations import Create_proposal
import pytest

import test_tools as tt
from . import test_utils
from .... import hive_utils

CREATOR: Final[str] = "initminer"
TREASURY: Final[str] = "hive.fund"


@pytest.fixture
def node() -> tt.InitNode:
    """
    Some tests in decentralized_hive_fund_tests produces around 90k blocks which results in a large size of
    `p2p.log` and 'stderr.txt' logs that weighs excessively around 500mb compressed. This fixture overrides the default
    `node` fixture to reduce the size of the logs, by setting the problematic loggers to a higher log level.
    """
    node = tt.InitNode()
    node.config.log_logger = (
        '{"name":"default","level":"error","appender":"stderr"} '
        '{"name":"user","level":"debug","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"debug","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )
    node.run()
    return node


def create_proposals(node, accounts, start_date, end_date, wif=None):
    tt.logger.info("Creating proposals...")
    for account in accounts:
        tt.logger.info(
            "New proposal ==> ({},{},{},{},{},{},{})".format(
                account["name"],
                account["name"],
                start_date,
                end_date,
                "24.000 TBD",
                "Proposal from account {}".format(account["name"]),
                test_utils.get_permlink(account["name"]),
            )
        )
        op = Create_proposal(
            **{
                "creator": account["name"],
                "receiver": account["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "24.000 TBD",
                "subject": "Proposal from account {}".format(account["name"]),
                "permlink": test_utils.get_permlink(account["name"]),
            }
        )
        node.finalizeOp(op, account["name"], "active")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)
