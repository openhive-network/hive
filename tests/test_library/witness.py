class Witness:
    key_generator = None

    def __init__(self, name, secret='secret'):
        if self.key_generator is None:
            raise Exception('Missing key generator, set it in Witness.key_generator')

        output = self.key_generator.generate_keys(name, secret)

        self.name = name
        self.secret = secret
        self.private_key = output['private_key']
        self.public_key = output['public_key']
