from typing import TYPE_CHECKING, Union

from test_tools.private.user_handles.get_implementation import get_implementation
from test_tools.wallet import Wallet

if TYPE_CHECKING:
    from test_tools import RemoteNode
    from test_tools.private.node import Node


class WalletHandle:
    def __init__(self, attach_to: Union[None, 'Node', 'RemoteNode'] = None):
        # Break import-cycle
        from test_tools import RemoteNode  # pylint: disable=import-outside-toplevel
        if isinstance(attach_to, RemoteNode):
            attach_to = get_implementation(attach_to)

        self.__implementation = Wallet(attach_to=attach_to)
        self.api = self.__implementation.api

    def in_single_transaction(self, *, broadcast=None):
        return self.__implementation.in_single_transaction(broadcast=broadcast)
