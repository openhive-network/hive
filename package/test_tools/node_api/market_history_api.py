from .api_base import ApiBase


class MarketHistoryApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'market_history_api')
