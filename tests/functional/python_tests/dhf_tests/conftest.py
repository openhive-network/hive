from typing import Final, List, Protocol

from beem import Hive
from beembase.operations import Create_proposal
import pytest
import test_tools as tt

from . import test_utils
from ... import hive_utils

CREATOR: Final[str] = "initminer"
TREASURY: Final[str] = "hive.fund"


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


@pytest.fixture
def node():
    init_node = tt.InitNode()
    init_node.run()
    return init_node


class NodeClientMaker(Protocol):
    def __call__(self, accounts: List[dict] = None) -> Hive:
        pass


@pytest.fixture
def node_client(node, worker_id) -> NodeClientMaker:
    def _node_client(accounts: List[dict] = None) -> Hive:
        accounts = accounts or []

        wif = tt.Account("initminer").private_key
        keys = [wif]

        for account in accounts:
            keys.append(account["private_key"])

        node_url = f"http://{node.http_endpoint}"
        return Hive(node=node_url, no_broadcast=False, keys=keys, profile=worker_id, num_retries=-1)

    return _node_client
