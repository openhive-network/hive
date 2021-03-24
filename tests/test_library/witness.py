class Witness:
    key_generator = None

    def __init__(self, name, secret='secret'):
        if Witness.key_generator is None:
            from .key_generator import KeyGenerator
            Witness.key_generator = KeyGenerator()

        output = Witness.key_generator.generate_keys(name, secret)

        self.name = name
        self.secret = secret
        self.private_key = output['private_key']
        self.public_key = output['public_key']
