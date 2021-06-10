class Account:
    key_generator = None

    def __init__(self, name, secret='secret'):
        if Account.key_generator is None:
            from .private.key_generator import KeyGenerator
            Account.key_generator = KeyGenerator()

        output = Account.key_generator.generate_keys(name, secret)

        self.name = name
        self.secret = secret
        self.private_key = output['private_key']
        self.public_key = output['public_key']
