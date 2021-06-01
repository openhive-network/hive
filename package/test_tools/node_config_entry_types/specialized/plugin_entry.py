from ..untouched_entry import Untouched


class Plugin(Untouched):
    SUPPORTED_PLUGINS = [
        'account_by_key', 'account_by_key_api', 'account_history', 'account_history_api', 'account_history_rocksdb',
        'block_api', 'block_data_export', 'block_log_info', 'chain', 'chain_api', 'comment_cashout_logging',
        'condenser_api', 'database_api', 'debug_node', 'debug_node_api', 'follow', 'follow_api', 'json_rpc',
        'market_history', 'market_history_api', 'network_broadcast_api', 'network_node_api', 'p2p', 'rc', 'rc_api',
        'reputation', 'reputation_api', 'rewards_api', 'state_snapshot', 'stats_export', 'statsd', 'tags',
        'tags_api', 'transaction_status', 'transaction_status_api', 'webserver', 'witness',
    ]

    @classmethod
    def __check_if_plugin_is_supported(cls, plugin):
        if plugin not in cls.SUPPORTED_PLUGINS:
            raise ValueError(
                f'Plugin "{plugin}" is not supported.\n'
                + 'List of supported plugins:\n'
                + '\n'.join([f'- {supported}' for supported in cls.SUPPORTED_PLUGINS])
            )

    @classmethod
    def _validate(cls, value):
        super()._validate(value)

        if value is not None:
            cls.__check_if_plugin_is_supported(value)

    def parse_from_text(self, plugin):
        self.__check_if_plugin_is_supported(plugin)
        return super().parse_from_text(plugin)
