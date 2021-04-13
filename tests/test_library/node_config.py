from .node_config_entry_types import *


class NodeConfig:
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

        # For IDE to support member names hints
        self.log_appender = None
        self.log_console_appender = None
        self.log_file_appender = None
        self.log_logger = None
        self.backtrace = None
        self.plugin = None
        self.account_history_track_account_range = None
        self.track_account_range = None
        self.account_history_whitelist_ops = None
        self.history_whitelist_ops = None
        self.account_history_blacklist_ops = None
        self.history_blacklist_ops = None
        self.history_disable_pruning = None
        self.account_history_rocksdb_path = None
        self.account_history_rocksdb_track_account_range = None
        self.account_history_rocksdb_whitelist_ops = None
        self.account_history_rocksdb_blacklist_ops = None
        self.block_data_export_file = None
        self.block_log_info_print_interval_seconds = None
        self.block_log_info_print_irreversible = None
        self.block_log_info_print_file = None
        self.shared_file_dir = None
        self.shared_file_size = None
        self.shared_file_full_threshold = None
        self.shared_file_scale_rate = None
        self.checkpoint = None
        self.flush_state_interval = None
        self.cashout_logging_starting_block = None
        self.cashout_logging_ending_block = None
        self.cashout_logging_log_path_dir = None
        self.debug_node_edit_script = None
        self.edit_script = None
        self.follow_max_feed_size = None
        self.follow_start_feeds = None
        self.log_json_rpc = None
        self.market_history_bucket_size = None
        self.market_history_buckets_per_size = None
        self.p2p_endpoint = None
        self.p2p_max_connections = None
        self.seed_node = None
        self.p2p_seed_node = None
        self.p2p_parameters = None
        self.rc_skip_reject_not_enough_rc = None
        self.rc_compute_historical_rc = None
        self.rc_start_at_block = None
        self.rc_account_whitelist = None
        self.snapshot_root_dir = None
        self.statsd_endpoint = None
        self.statsd_batchsize = None
        self.statsd_whitelist = None
        self.statsd_blacklist = None
        self.tags_start_promoted = None
        self.tags_skip_startup_update = None
        self.transaction_status_block_depth = None
        self.transaction_status_track_after_block = None
        self.webserver_http_endpoint = None
        self.webserver_unix_endpoint = None
        self.webserver_ws_endpoint = None
        self.webserver_enable_permessage_deflate = None
        self.rpc_endpoint = None
        self.webserver_thread_pool_size = None
        self.enable_stale_production = None
        self.required_participation = None
        self.witness = None
        self.private_key = None
        self.witness_skip_enforce_bandwidth = None

        entries = super().__getattribute__('entries')
        entries['plugin'] = List(Plugin)
        entries['witness'] = List(String, single_line=False)
        entries['private_key'] = List(Untouched, single_line=False)
        for key in ['shared_file_dir', 'account_history_rocksdb_path', 'snapshot_root_dir']:
            entries[key] = String()

    def __check_if_key_is_valid(self, key):
        entries = super().__getattribute__('entries')
        if key not in entries.keys():
            raise AttributeError('Wrong config item name')

    def __setattr__(self, key, value):
        self.__check_if_key_is_valid(key)

        entries = super().__getattribute__('entries')
        entries[key].value = value

    def __getattr__(self, key):
        self.__check_if_key_is_valid(key)

        entries = super().__getattribute__('entries')
        return entries[key].value

    def __str__(self):
        supported_entries = super().__getattribute__('SUPPORTED_ENTRIES')
        return '\n'.join([f'{key}={item.value}' for key, item in supported_entries.items()])

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            raise Exception('Comparison with unsupported type')

        entries = super().__getattribute__('entries')
        return all([item.value == getattr(other, key) for key, item in entries.items()])

    def __ne__(self, other):
        return not self == other

    def get_differences_between(self, other):
        if self == other:
            return None

        differences = {}
        supported_entries = super().__getattribute__('SUPPORTED_ENTRIES')
        for member in supported_entries:
            mine = getattr(self, member)
            his = getattr(other, member)

            if mine != his:
                differences[member] = (mine, his)

        return differences

    def write_to_lines(self):
        file_entries = []
        for key, entry in self.entries.items():
            if not entry.value:
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

        for line in lines:
            if is_entry_line(line):
                key, value = parse_entry_line(line)
                entries = super().__getattribute__('entries')
                entries[key.replace('-', '_')].parse_from_text(value)

    def load_from_file(self, file_path):
        with open(file_path) as file:
            self.load_from_lines(file.readlines())
