from typing import TYPE_CHECKING, Union
from test_tools.wallet import Wallet

if TYPE_CHECKING:
    from test_tools.private.node import Node
    from test_tools.private.remote_node import RemoteNode


class WalletHandle:
    def __init__(self, attach_to: Union[None, 'Node', 'RemoteNode'] = None):
        self.__implementation = Wallet(attach_to=attach_to)
        self.api = self.__implementation.api

    def in_single_transaction(self, *, broadcast=None):
        return self.__implementation.in_single_transaction(broadcast=broadcast)
