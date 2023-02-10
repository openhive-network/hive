from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT


@run_for("testnet")
def test_decline_voting_rights_more_than_once_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
