from .account_by_key_api import AccountByKeyApi
from .account_history_api import AccountHistoryApi
from .block_api import BlockApi
from .bridge_api import BridgeApi
from .condenser_api import CondenserApi
from .database_api import DatabaseApi
from .debug_node_api import DebugNodeApi
from .follow_api import FollowApi
from .jsonrpc_api import JsonrpcApi
from .market_history_api import MarketHistoryApi
from .network_broadcast_api import NetworkBroadcastApi
from .network_node_api import NetworkNodeApi
from .rc_api import RcApi
from .reputation_api import ReputationApi
from .rewards_api import RewardsApi
from .tags_api import TagsApi
from .transaction_status_api import TransactionStatusApi
from .witness_api import WitnessApi


class Apis:
    def __init__(self, node):
        self.__node = node

        self.account_by_key = AccountByKeyApi(self.__node)
        self.account_history = AccountHistoryApi(self.__node)
        self.block = BlockApi(self.__node)
        self.bridge = BridgeApi(self.__node)
        self.condenser = CondenserApi(self.__node)
        self.database = DatabaseApi(self.__node)
        self.debug_node = DebugNodeApi(self.__node)
        self.follow = FollowApi(self.__node)
        self.jsonrpc = JsonrpcApi(self.__node)
        self.market_history = MarketHistoryApi(self.__node)
        self.network_broadcast = NetworkBroadcastApi(self.__node)
        self.network_node = NetworkNodeApi(self.__node)
        self.rc = RcApi(self.__node)
        self.reputation = ReputationApi(self.__node)
        self.rewards = RewardsApi(self.__node)
        self.tags = TagsApi(self.__node)
        self.transaction_status = TransactionStatusApi(self.__node)
        self.witness = WitnessApi(self.__node)
