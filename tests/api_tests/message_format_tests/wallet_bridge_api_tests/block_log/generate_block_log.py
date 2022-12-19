from pathlib import Path

import test_tools as tt

WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer

ACCOUNTS = [f'account-{i}' for i in range(10)]


def prepare_block_log_with_witnesses():
    node, wallet = prepare_node_with_witnesses(WITNESSES_NAMES)

    wallet.create_accounts(len(ACCOUNTS))

    node.wait_for_irreversible_block()

    node.close()

    node.block_log.copy_to(Path(__file__).parent.absolute())


def prepare_node_with_witnesses(witnesses_names):
    node = tt.InitNode()
    for name in witnesses_names:
        witness = tt.Account(name)
        node.config.witness.append(witness.name)
        node.config.private_key.append(witness.private_key)

    node.run()
    wallet = tt.Wallet(attach_to=node)

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.create_account('initminer', name, '')

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))

    with wallet.in_single_transaction():
        for name in witnesses_names:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    tt.logger.info('Waiting for next witness schedule...')
    node.wait_for_block_with_number(22+21) # activation of HF26 makes current schedule also a future one,
                                           # so we have to wait two terms for the witnesses to activate

    return node, wallet


if __name__ == '__main__':
    prepare_block_log_with_witnesses()
