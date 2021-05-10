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
        self.__clear()

        entries = super().__getattribute__('entries')
        entries['plugin'] = List(Plugin)
        entries['p2p_endpoint'] = Untouched()
        entries['p2p_seed_node'] = List(Untouched)
        entries['webserver_http_endpoint'] = Untouched()
        entries['webserver_unix_endpoint'] = Untouched()
        entries['webserver_ws_endpoint'] = Untouched()
        entries['enable_stale_production'] = Boolean()
        entries['required_participation'] = Integer()
        entries['witness'] = WitnessList(self)
        entries['private_key'] = List(Untouched, single_line=False)
        for key in ['shared_file_dir', 'account_history_rocksdb_path', 'snapshot_root_dir']:
            entries[key] = String()

    def __clear(self):
        # For IDE to support member names hints
        self.log_appender.clear()
        self.log_console_appender.clear()
        self.log_file_appender.clear()
        self.log_logger.clear()
        self.backtrace.clear()
        self.plugin.clear()
        self.account_history_track_account_range.clear()
        self.track_account_range.clear()
        self.account_history_whitelist_ops.clear()
        self.history_whitelist_ops.clear()
        self.account_history_blacklist_ops.clear()
        self.history_blacklist_ops.clear()
        self.history_disable_pruning.clear()
        self.account_history_rocksdb_path.clear()
        self.account_history_rocksdb_track_account_range.clear()
        self.account_history_rocksdb_whitelist_ops.clear()
        self.account_history_rocksdb_blacklist_ops.clear()
        self.block_data_export_file.clear()
        self.block_log_info_print_interval_seconds.clear()
        self.block_log_info_print_irreversible.clear()
        self.block_log_info_print_file.clear()
        self.shared_file_dir.clear()
        self.shared_file_size.clear()
        self.shared_file_full_threshold.clear()
        self.shared_file_scale_rate.clear()
        self.checkpoint.clear()
        self.flush_state_interval.clear()
        self.cashout_logging_starting_block.clear()
        self.cashout_logging_ending_block.clear()
        self.cashout_logging_log_path_dir.clear()
        self.debug_node_edit_script.clear()
        self.edit_script.clear()
        self.follow_max_feed_size.clear()
        self.follow_start_feeds.clear()
        self.log_json_rpc.clear()
        self.market_history_bucket_size.clear()
        self.market_history_buckets_per_size.clear()
        self.p2p_endpoint.clear()
        self.p2p_max_connections.clear()
        self.seed_node.clear()
        self.p2p_seed_node.clear()
        self.p2p_parameters.clear()
        self.rc_skip_reject_not_enough_rc.clear()
        self.rc_compute_historical_rc.clear()
        self.rc_start_at_block.clear()
        self.rc_account_whitelist.clear()
        self.snapshot_root_dir.clear()
        self.statsd_endpoint.clear()
        self.statsd_batchsize.clear()
        self.statsd_whitelist.clear()
        self.statsd_blacklist.clear()
        self.tags_start_promoted.clear()
        self.tags_skip_startup_update.clear()
        self.transaction_status_block_depth.clear()
        self.transaction_status_track_after_block.clear()
        self.webserver_http_endpoint.clear()
        self.webserver_unix_endpoint.clear()
        self.webserver_ws_endpoint.clear()
        self.webserver_enable_permessage_deflate.clear()
        self.rpc_endpoint.clear()
        self.webserver_thread_pool_size.clear()
        self.enable_stale_production.clear()
        self.required_participation.clear()
        self.witness.clear()
        self.private_key.clear()
        self.witness_skip_enforce_bandwidth.clear()

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
        entries[key].set_value(value)

    def __getattr__(self, key):
        self.__check_if_key_internal_is_valid(key)

        entries = super().__getattribute__('entries')
        return entries[key].get_value()

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
