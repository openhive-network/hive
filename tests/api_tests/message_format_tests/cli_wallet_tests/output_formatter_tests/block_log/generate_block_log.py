from pathlib import Path

import test_tools as tt

BALANCES = {
    'bob': {
        'hives': tt.Asset.Test(100),
        'vests': tt.Asset.Test(110),
        'hbds': tt.Asset.Tbd(120),
    },
    'carol': {
        'hives': tt.Asset.Test(200),
        'vests': tt.Asset.Test(210),
        'hbds': tt.Asset.Tbd(220),
    },
    'dan': {
        'hives': tt.Asset.Test(300),
        'vests': tt.Asset.Test(310),
        'hbds': tt.Asset.Tbd(320),
    }
}


def prepare_block_log_with_transactions():
    node = tt.InitNode()
    node.run()

    wallet = tt.Wallet(attach_to=node)

    prepare_transactions_for_get_account_history_test(wallet)
    prepare_accounts_for_list_my_accounts_test(wallet)

    node.wait_for_irreversible_block()

    node.close()

    node.block_log.copy_to(Path(__file__).parent.absolute())


def prepare_transactions_for_get_account_history_test(wallet) -> None:
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.post_comment('alice', 'test-permlink', '', 'someone0', 'test-title', 'this is a body', '{}')

    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)


def prepare_accounts_for_list_my_accounts_test(wallet) -> None:
    for name, balances in BALANCES.items():
        wallet.create_account(name, **balances)


if __name__ == '__main__':
    prepare_block_log_with_transactions()
