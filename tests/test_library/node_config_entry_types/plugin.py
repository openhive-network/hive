from .untouched import Untouched


class Plugin(Untouched):
    def __init__(self):
        super().__init__()

    @staticmethod
    def __check_if_plugin_is_supported(plugin):
        supported_plugins = [
            'account_by_key', 'account_by_key_api', 'account_history', 'account_history_api', 'account_history_rocksdb',
            'block_api', 'block_data_export', 'block_log_info', 'chain', 'chain_api', 'comment_cashout_logging',
            'condenser_api', 'database_api', 'debug_node', 'debug_node_api', 'follow', 'follow_api', 'json_rpc',
            'market_history', 'market_history_api', 'network_broadcast_api', 'network_node_api', 'p2p', 'rc', 'rc_api',
            'reputation', 'reputation_api', 'rewards_api', 'state_snapshot', 'stats_export', 'statsd', 'tags',
            'tags_api', 'test_api', 'transaction_status', 'transaction_status_api', 'webserver', 'witness',
        ]

        if plugin not in supported_plugins:
            raise Exception(f'Plugin {plugin} is not supported')

    def parse_from_text(self, text):
        super().parse_from_text(text)
        self.__check_if_plugin_is_supported(super().value)

    @property
    def value(self):
        return super().value

    @value.setter
    def value(self, value):
        super().value = value
        self.__check_if_plugin_is_supported(super().value)
