from .api_base import ApiBase


class DatabaseApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'database_api')
