from test_tools.node_api.account_by_key_api import AccountByKeyApi
from test_tools.node_api.account_history_api import AccountHistoryApi
from test_tools.node_api.block_api import BlockApi
from test_tools.node_api.bridge_api import BridgeApi
from test_tools.node_api.condenser_api import CondenserApi
from test_tools.node_api.database_api import DatabaseApi
from test_tools.node_api.debug_node_api import DebugNodeApi
from test_tools.node_api.jsonrpc_api import JsonrpcApi
from test_tools.node_api.market_history_api import MarketHistoryApi
from test_tools.node_api.network_broadcast_api import NetworkBroadcastApi
from test_tools.node_api.network_node_api import NetworkNodeApi
from test_tools.node_api.rc_api import RcApi
from test_tools.node_api.reputation_api import ReputationApi
from test_tools.node_api.rewards_api import RewardsApi
from test_tools.node_api.transaction_status_api import TransactionStatusApi
from test_tools.node_api.witness_api import WitnessApi


class Apis:
    # pylint: disable=too-many-instance-attributes
    # Node contains so many APIs and all of them must be defined here

    def __init__(self, node):
        self.__node = node

        self.account_by_key = AccountByKeyApi(self.__node)
        self.account_history = AccountHistoryApi(self.__node)
        self.block = BlockApi(self.__node)
        self.bridge = BridgeApi(self.__node)
        self.condenser = CondenserApi(self.__node)
        self.database = DatabaseApi(self.__node)
        self.debug_node = DebugNodeApi(self.__node)
        self.jsonrpc = JsonrpcApi(self.__node)
        self.market_history = MarketHistoryApi(self.__node)
        self.network_broadcast = NetworkBroadcastApi(self.__node)
        self.network_node = NetworkNodeApi(self.__node)
        self.rc = RcApi(self.__node)  # pylint: disable=invalid-name
        self.reputation = ReputationApi(self.__node)
        self.rewards = RewardsApi(self.__node)
        self.transaction_status = TransactionStatusApi(self.__node)
        self.witness = WitnessApi(self.__node)
