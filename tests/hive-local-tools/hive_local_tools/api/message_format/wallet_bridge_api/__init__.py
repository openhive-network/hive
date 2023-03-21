import test_tools as tt


def create_account_and_create_order(wallet, account_name):
    wallet.api.create_account('initminer', account_name, '{}')
    wallet.api.transfer('initminer', account_name, tt.Asset.Test(100), 'memo')
    wallet.api.transfer_to_vesting('initminer', account_name, tt.Asset.Test(100))
    wallet.api.create_order(account_name, 1000, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)


def create_accounts_with_vests_and_tbd(wallet, accounts):
    created_accounts = wallet.create_accounts(len(accounts))
    assert get_accounts_name(created_accounts) == accounts

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account, tt.Asset.Test(10000))

    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer('initminer', account, tt.Asset.Tbd(10000), 'memo')


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def prepare_proposals(wallet, accounts):
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.post_comment(account, 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    with wallet.in_single_transaction():
        for account_number in range(len(accounts)):
            wallet.api.create_proposal(accounts[account_number],
                                       accounts[account_number],
                                       tt.Time.from_now(weeks=10),
                                       tt.Time.from_now(weeks=15),
                                       tt.Asset.Tbd(account_number * 100),
                                       f'subject-{account_number}',
                                       'permlink')


def prepare_node_with_witnesses(witnesses_names):
    node = tt.InitNode()
    for name in witnesses_names:
        witness = tt.Account(name)
        node.config.witness.append(witness.name)
        node.config.private_key.append(witness.private_key)

    node.run(time_offset="+0h x50")
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
    wallet.close()
    tt.logger.info('Waiting for next witness schedule...')
    node.wait_for_block_with_number(22+21) # activation of HF26 makes current schedule also a future one,
                                           # so we have to wait two terms for the witnesses to activate

    return node


def get_transaction_id_from_head_block(node) -> str:
    if isinstance(node, (tt.ApiNode, tt.FullApiNode, tt.InitNode, tt.RemoteNode)):
        head_block_number = node.api.database.get_dynamic_global_properties()["head_block_number"]
        if not isinstance(node, tt.RemoteNode):
            node.wait_for_irreversible_block()
        transaction_id = node.api.block.get_block(block_num=head_block_number)["block"]["transaction_ids"][0]
        return transaction_id
