from test_tools import Account


def test_multiple_accounts_creation():
    accounts = Account.create_multiple(3, 'example')
    assert [f'example-{i}' for i in range(3)] == [account.name for account in accounts]
