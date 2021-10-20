from test_tools import Account, constants, logger, Wallet


def test_create_account_snapshot(world):
    node = world.create_init_node()
    node.config.shared_file_size = '32G'
    node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    node.run()
    wallet = Wallet(attach_to=node)

    number_of_accounts_in_one_transaction = 400
    number_of_transactions = 2500
    number_of_accounts = number_of_accounts_in_one_transaction * number_of_transactions
    account_number_absolute = 0

    accounts = Account.create_multiple(number_of_accounts, 'receiver')
    for number_of_transaction in range(number_of_transactions):
        with wallet.in_single_transaction():
            for account_number in range(number_of_accounts_in_one_transaction):
                account_name = accounts[account_number_absolute].name
                account_key = accounts[account_number_absolute].public_key
                wallet.api.create_account_with_keys('initminer', account_name, '{}', account_key, account_key,
                                                    account_key, account_key)
                account_number_absolute = account_number_absolute + 1
    logger.info('End account creations')
    snapshot = node.dump_snapshot()
