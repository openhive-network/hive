from .api_base import ApiBase


class AccountHistoryApi(ApiBase):
    def __init__(self, node):
        super().__init__(node, 'account_history_api')

    def get_ops_in_block(self, block_num, only_virtual):
        return self._send(
            'get_ops_in_block',
            {
                'block_num': block_num,
                'only_virtual': only_virtual,
            }
        )

    def enum_virtual_ops(self, block_range_begin, block_range_end, include_reversible=False):
        return self._send(
            'enum_virtual_ops',
            {
                'block_range_begin': block_range_begin,
                'block_range_end': block_range_end,
                'include_reversible': include_reversible
            }
        )
