from .api_base import ApiBase


class CondenserApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'condenser_api')
