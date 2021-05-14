from .api_base import ApiBase


class CondenserApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'condenser_api')

    def get_account_history(self, account, start, limit):
        return self._send('get_account_history', [account, start, limit])

    def get_key_references(self, keys):
        return self._send('get_key_references', keys)
