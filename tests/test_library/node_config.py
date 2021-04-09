class NodeConfig:
    __slots__ = [
        'entries',  # Temporary workaround
        'log_appender', 'log_console_appender', 'log_file_appender', 'log_logger', 'backtrace', 'plugin',
        'account_history_track_account_range', 'track_account_range', 'account_history_whitelist_ops',
        'history_whitelist_ops', 'account_history_blacklist_ops', 'history_blacklist_ops', 'history_disable_pruning',
        'account_history_rocksdb_path', 'account_history_rocksdb_track_account_range',
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
        'webserver_ws_endpoint', 'webserver_enable_permessage_deflate', 'rpc_endpoint', 'webserver_thread_pool_size',
        'enable_stale_production', 'required_participation', 'witness', 'private_key', 'witness_skip_enforce_bandwidth',
    ]

    class Entry:
        def __init__(self, value, description='description'):
            self.values = {value}
            self.description = description

        def __str__(self):
            result = ' '.join(self.values)

            if self.description:
                max_length = 32

                if len(self.description) > max_length:
                    description = self.description[:max_length] + '(...)'
                else:
                    description = self.description

                result += f' # {description}'

            return result

    def __init__(self):
        for member in self.__slots__:
            setattr(self, member, None)

        self.entries = {}

    def __str__(self):
        return '\n'.join([f'{key}={str(entry)}' for key, entry in self.entries.items()])

    def __contains__(self, key):
        return key in self.entries

    def __getitem__(self, key):
        return list(self.entries[key].values) if key in self.entries else []

    def add_entry(self, key, value, description=None):
        if key not in self.entries:
            self.entries[key] = self.Entry(value, description)
            return

        entry = self.entries[key]
        entry.values.add(value)

    def write_to_file(self, file_path):
        file_entries = []
        with open(file_path, 'w') as file:
            for key, entry in self.entries.items():
                file_entry = f'# {entry.description}\n' if entry.description else ''

                if key in ['private-key', 'witness']:
                    for value in entry.values:
                        file_entry += f'{key} = {value}\n'
                else:
                    file_entry += f'{key} = '
                    file_entry += ' '.join(entry.values)
                    file_entry += '\n'

                file_entries.append(file_entry)

            file.write('\n'.join(file_entries))
