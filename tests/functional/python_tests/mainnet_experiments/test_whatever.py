from test_tools import logger, Wallet


def test_whatever(node):
    wallet = Wallet(attach_to=node)
    accounts = wallet.list_accounts()
    logger.info(f'{len(accounts)=}')
