from .. import List, String


class WitnessList(List):
    class _BaseProxyType(List._BaseProxyType):
        def append(self, witness, with_key=True):
            self.entry._value.append(witness)

            if with_key:
                from test_library import Account
                self.entry._append_private_key(Account(witness).private_key)

        def extend(self, witnesses, with_key=True):
            for witness in witnesses:
                self.append(witness, with_key)

    class _UnsetProxyType(_BaseProxyType, List._UnsetProxy):
        pass

    class _ValueProxyType(_BaseProxyType, List._ValueProxy):
        pass

    _UnsetProxy = _UnsetProxyType
    _ValueProxy = _ValueProxyType

    def __init__(self, config):
        super().__init__(String, single_line=False)
        self.__config = config

    def _append_private_key(self, key):
        self.__config.private_key.append(key)
