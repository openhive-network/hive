from test_tools.private.key_generator import KeyGenerator


class Account:
    key_generator = KeyGenerator()

    def __init__(self, name, secret='secret', with_keys=True):
        self.name = name
        self.secret = None
        self.private_key = None
        self.public_key = None

        if with_keys:
            output = self.key_generator.generate_keys(name, secret=secret)[0]
            self.secret = secret
            self.private_key = output['private_key']
            self.public_key = output['public_key']

    @classmethod
    def create_multiple(cls, number_of_accounts, name_base='account', *, secret='secret'):
        accounts = []
        for generated in cls.key_generator.generate_keys(name_base, number_of_accounts=number_of_accounts, secret=secret):
            account = Account(generated['account_name'], with_keys=False)
            account.secret = secret
            account.private_key = generated['private_key']
            account.public_key = generated['public_key']

            accounts.append(account)

        return accounts
