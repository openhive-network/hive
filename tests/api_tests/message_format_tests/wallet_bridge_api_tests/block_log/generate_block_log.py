from pathlib import Path

from ..local_tools import prepare_node_with_witnesses

WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer

ACCOUNTS = [f'account-{i}' for i in range(10)]


def prepare_block_log_with_witnesses():
    node, wallet = prepare_node_with_witnesses(WITNESSES_NAMES)

    wallet.create_accounts(len(ACCOUNTS))

    node.wait_number_of_blocks(21)

    node.close()

    node.get_block_log(include_index=True).copy_to(Path(__file__).parent.absolute())


if __name__ == '__main__':
    prepare_block_log_with_witnesses()
