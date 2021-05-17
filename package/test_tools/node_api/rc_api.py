from .api_base import ApiBase


class RcApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'rc_api')
