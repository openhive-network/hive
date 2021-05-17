from .api_base import ApiBase


class AccountByKeyApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'account_by_key_api')
