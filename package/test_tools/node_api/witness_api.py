from .api_base import ApiBase


class WitnessApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'witness_api')
