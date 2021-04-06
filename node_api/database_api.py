from .api_base import ApiBase


class DatabaseApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'database_api')

    def list_witnesses(self, limit, order, start):
        return self._send(
            'list_witnesses',
            {
                'limit': limit,
                'order': order,
                'start': start,
            }
        )
