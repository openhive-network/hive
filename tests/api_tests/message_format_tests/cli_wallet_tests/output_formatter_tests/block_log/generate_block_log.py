from pathlib import Path

import test_tools as tt


def prepare_block_log_with_transactions():
    node = tt.InitNode()
    node.run()

    wallet = tt.Wallet(attach_to=node)

    prepare_transactions(wallet)

    # Wait to appear transactions in block log
    node.wait_number_of_blocks(21)

    node.close()

    node.get_block_log(include_index=False).copy_to(Path(__file__).parent.absolute())


def prepare_transactions(wallet) -> None:
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.post_comment('alice', 'test-permlink', '', 'someone0', 'test-title', 'this is a body', '{}')

    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)


if __name__ == '__main__':
    prepare_block_log_with_transactions()
