from .api_base import ApiBase


class RewardsApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'rewards_api')
