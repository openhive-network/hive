from .untouched import Untouched


class Plugin(Untouched):
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
            raise Exception(
                f'Plugin "{plugin}" is not supported.\n'
                + 'List of supported plugins:\n'
                + '\n'.join([f'- {supported}' for supported in supported_plugins])
            )

    def _parse_from_text(self, plugin):
        self.__check_if_plugin_is_supported(plugin)
        super()._parse_from_text(plugin)

    def _set_value(self, value):
        self.__check_if_plugin_is_supported(value)
        super()._set_value(value)
