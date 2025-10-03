import test_tools as tt
from hive_local_tools.functional import connect_nodes


def test_issue_794() -> None:
    init_node = tt.InitNode()
    api_node = tt.ApiNode()

    init_node.run()
    connect_nodes(init_node, api_node)

    for restart_number in range(5):
        api_node.run()
        api_node.wait_for_live_mode()

        while not api_node.get_last_irreversible_block_number():
            api_node.wait_number_of_blocks(1)
            lib = api_node.get_last_irreversible_block_number()

        while api_node.get_last_irreversible_block_number() <= lib:
            api_node.wait_number_of_blocks(1)

        api_node.wait_number_of_blocks(3)
        api_node.close()
