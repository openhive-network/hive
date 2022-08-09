from ..local_tools import run_for


# This test is not performed on 5 million block log because reward fund feature was introduced after block with number
# 5000000. See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_get_reward_fund(prepared_node):
    funds = prepared_node.api.condenser.get_reward_fund('post')
    assert len(funds) != 0
