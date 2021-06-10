from test_tools.private.key_generator import KeyGenerator


class Account:
    key_generator = KeyGenerator()

    def __init__(self, name, secret='secret'):
        output = self.key_generator.generate_keys(name, secret)

        self.name = name
        self.secret = secret
        self.private_key = output['private_key']
        self.public_key = output['public_key']
