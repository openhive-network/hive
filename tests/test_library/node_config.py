from .node_config_entry_types import *


class NodeConfig:
    class __UnsetType:
        def __iadd__(self, other):
            return other

        def __bool__(self):
            return False

    UNSET = __UnsetType()

    def __init__(self):
        super().__setattr__('SUPPORTED_ENTRIES', [
            'log_appender', 'log_console_appender', 'log_file_appender', 'log_logger', 'backtrace', 'plugin',
            'account_history_track_account_range', 'track_account_range', 'account_history_whitelist_ops',
            'history_whitelist_ops', 'account_history_blacklist_ops', 'history_blacklist_ops',
            'history_disable_pruning', 'account_history_rocksdb_path', 'account_history_rocksdb_track_account_range',
            'account_history_rocksdb_whitelist_ops', 'account_history_rocksdb_blacklist_ops', 'block_data_export_file',
            'block_log_info_print_interval_seconds', 'block_log_info_print_irreversible', 'block_log_info_print_file',
            'shared_file_dir', 'shared_file_size', 'shared_file_full_threshold', 'shared_file_scale_rate', 'checkpoint',
            'flush_state_interval', 'cashout_logging_starting_block', 'cashout_logging_ending_block',
            'cashout_logging_log_path_dir', 'debug_node_edit_script', 'edit_script', 'follow_max_feed_size',
            'follow_start_feeds', 'log_json_rpc', 'market_history_bucket_size', 'market_history_buckets_per_size',
            'p2p_endpoint', 'p2p_max_connections', 'seed_node', 'p2p_seed_node', 'p2p_parameters',
            'rc_skip_reject_not_enough_rc', 'rc_compute_historical_rc', 'rc_start_at_block', 'rc_account_whitelist',
            'snapshot_root_dir', 'statsd_endpoint', 'statsd_batchsize', 'statsd_whitelist', 'statsd_blacklist',
            'tags_start_promoted', 'tags_skip_startup_update', 'transaction_status_block_depth',
            'transaction_status_track_after_block', 'webserver_http_endpoint', 'webserver_unix_endpoint',
            'webserver_ws_endpoint', 'webserver_enable_permessage_deflate', 'rpc_endpoint',
            'webserver_thread_pool_size', 'enable_stale_production', 'required_participation', 'witness', 'private_key',
            'witness_skip_enforce_bandwidth',
        ])
        supported_entries = super().__getattribute__('SUPPORTED_ENTRIES')

        super().__setattr__('entries', {entry: Untouched() for entry in supported_entries})
        self.__clear()

        entries = super().__getattribute__('entries')
        entries['plugin'] = List(Plugin)
        entries['p2p_endpoint'] = Untouched()
        entries['p2p_seed_node'] = List(Untouched)
        entries['webserver_http_endpoint'] = Untouched()
        entries['webserver_unix_endpoint'] = Untouched()
        entries['webserver_ws_endpoint'] = Untouched()
        entries['witness'] = List(String, single_line=False)
        entries['private_key'] = List(Untouched, single_line=False)
        for key in ['shared_file_dir', 'account_history_rocksdb_path', 'snapshot_root_dir']:
            entries[key] = String()

    def __clear(self):
        # For IDE to support member names hints
        self.log_appender = self.UNSET
        self.log_console_appender = self.UNSET
        self.log_file_appender = self.UNSET
        self.log_logger = self.UNSET
        self.backtrace = self.UNSET
        self.plugin = self.UNSET
        self.account_history_track_account_range = self.UNSET
        self.track_account_range = self.UNSET
        self.account_history_whitelist_ops = self.UNSET
        self.history_whitelist_ops = self.UNSET
        self.account_history_blacklist_ops = self.UNSET
        self.history_blacklist_ops = self.UNSET
        self.history_disable_pruning = self.UNSET
        self.account_history_rocksdb_path = self.UNSET
        self.account_history_rocksdb_track_account_range = self.UNSET
        self.account_history_rocksdb_whitelist_ops = self.UNSET
        self.account_history_rocksdb_blacklist_ops = self.UNSET
        self.block_data_export_file = self.UNSET
        self.block_log_info_print_interval_seconds = self.UNSET
        self.block_log_info_print_irreversible = self.UNSET
        self.block_log_info_print_file = self.UNSET
        self.shared_file_dir = self.UNSET
        self.shared_file_size = self.UNSET
        self.shared_file_full_threshold = self.UNSET
        self.shared_file_scale_rate = self.UNSET
        self.checkpoint = self.UNSET
        self.flush_state_interval = self.UNSET
        self.cashout_logging_starting_block = self.UNSET
        self.cashout_logging_ending_block = self.UNSET
        self.cashout_logging_log_path_dir = self.UNSET
        self.debug_node_edit_script = self.UNSET
        self.edit_script = self.UNSET
        self.follow_max_feed_size = self.UNSET
        self.follow_start_feeds = self.UNSET
        self.log_json_rpc = self.UNSET
        self.market_history_bucket_size = self.UNSET
        self.market_history_buckets_per_size = self.UNSET
        self.p2p_endpoint = self.UNSET
        self.p2p_max_connections = self.UNSET
        self.seed_node = self.UNSET
        self.p2p_seed_node = self.UNSET
        self.p2p_parameters = self.UNSET
        self.rc_skip_reject_not_enough_rc = self.UNSET
        self.rc_compute_historical_rc = self.UNSET
        self.rc_start_at_block = self.UNSET
        self.rc_account_whitelist = self.UNSET
        self.snapshot_root_dir = self.UNSET
        self.statsd_endpoint = self.UNSET
        self.statsd_batchsize = self.UNSET
        self.statsd_whitelist = self.UNSET
        self.statsd_blacklist = self.UNSET
        self.tags_start_promoted = self.UNSET
        self.tags_skip_startup_update = self.UNSET
        self.transaction_status_block_depth = self.UNSET
        self.transaction_status_track_after_block = self.UNSET
        self.webserver_http_endpoint = self.UNSET
        self.webserver_unix_endpoint = self.UNSET
        self.webserver_ws_endpoint = self.UNSET
        self.webserver_enable_permessage_deflate = self.UNSET
        self.rpc_endpoint = self.UNSET
        self.webserver_thread_pool_size = self.UNSET
        self.enable_stale_production = self.UNSET
        self.required_participation = self.UNSET
        self.witness = self.UNSET
        self.private_key = self.UNSET
        self.witness_skip_enforce_bandwidth = self.UNSET

    def __check_if_key_internal_is_valid(self, key):
        """Internal keys (used inside this class) have underscores instead of hyphens"""

        supported_keys = super().__getattribute__('SUPPORTED_ENTRIES')
        if key not in supported_keys:
            raise AttributeError('Wrong config item name')

    def __check_if_key_from_file_is_valid(self, key):
        """Keys from file have hyphens instead of underscores (like internal keys)"""

        supported_keys = [entry.replace('_', '-') for entry in super().__getattribute__('SUPPORTED_ENTRIES')]
        if key not in supported_keys:
            raise AttributeError('Wrong config item name')

    def __setattr__(self, key, value):
        self.__check_if_key_internal_is_valid(key)

        entries = super().__getattribute__('entries')

        if value is self.UNSET:
            entries[key].clear()
            return

        entries[key].set_value(value)

    def __getattr__(self, key):
        self.__check_if_key_internal_is_valid(key)

        entries = super().__getattribute__('entries')
        if entries[key].is_set():
            return entries[key].get_value()

        return self.UNSET

    def __str__(self):
        supported_entries = super().__getattribute__('SUPPORTED_ENTRIES')
        return '\n'.join([f'{key}={item.get_value()}' for key, item in supported_entries.items()])

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            raise Exception('Comparison with unsupported type')
        return not self.get_differences_between(other, stop_at_first_difference=True)

    def __ne__(self, other):
        return not self == other

    def get_differences_between(self, other, stop_at_first_difference=False):
        differences = {}
        supported_entries = super().__getattribute__('SUPPORTED_ENTRIES')
        for member in supported_entries:
            mine = getattr(self, member)
            his = getattr(other, member)

            if mine != his:
                differences[member] = (mine, his)
                if stop_at_first_difference:
                    break

        return differences

    def write_to_lines(self):
        file_entries = []
        for key, entry in self.entries.items():
            if not entry.is_set():
                continue

            value = entry.serialize_to_text()
            value = value if isinstance(value, list) else [value]
            for v in value:
                file_entries.append(f"{key.replace('_', '-')} = {v}")

        return file_entries

    def write_to_file(self, file_path):
        with open(file_path, 'w') as file:
            file.write('\n'.join(self.write_to_lines()))

    def load_from_lines(self, lines):
        import re

        def parse_entry_line(line):
            result = re.match(r'^\s*([\w\-]+)\s*=\s*(.*?)\s*$', line)
            return (result[1], result[2]) if result is not None else None

        def is_entry_line(line):
            return parse_entry_line(line) is not None

        self.__clear()
        for line in lines:
            if is_entry_line(line):
                key, value = parse_entry_line(line)
                self.__check_if_key_from_file_is_valid(key)

                entries = super().__getattribute__('entries')
                entries[key.replace('-', '_')].parse_from_text(value)

    def load_from_file(self, file_path):
        with open(file_path) as file:
            self.load_from_lines(file.readlines())
