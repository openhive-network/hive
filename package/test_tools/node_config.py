import re

from test_tools.private.node_config_entry_types import Boolean, Integer, List, Plugin, String, Untouched


class NodeConfig:
    # pylint: disable=too-many-instance-attributes
    # Config contains so many entries and all of them must be defined here

    def __init__(self):
        self.__enter_initialization_stage()
        self.__initialize()
        self.__exit_initialization_stage()

    def __initialize(self):
        self.__define_entries()

    def __enter_initialization_stage(self):
        super().__setattr__('_initialization_stage', None)

    def __exit_initialization_stage(self):
        super().__delattr__('_initialization_stage')

    def __is_initialization_stage(self):
        return '_initialization_stage' in self.__dict__

    def __define_entries(self):
        # pylint: disable=too-many-statements
        # Config contains so many entries and all of them must be defined here

        super().__setattr__('__entries', {})

        # pylint: disable=attribute-defined-outside-init
        # This method is called in __init__
        self.log_appender = Untouched()  # Set correct type
        self.log_console_appender = Untouched()  # Set correct type
        self.log_file_appender = Untouched()  # Set correct type
        self.log_logger = Untouched()  # Set correct type
        self.backtrace = Untouched()  # Set correct type
        self.plugin = List(Plugin)
        self.account_history_track_account_range = Untouched()  # Set correct type
        self.track_account_range = Untouched()  # Set correct type
        self.account_history_whitelist_ops = Untouched()  # Set correct type
        self.history_whitelist_ops = Untouched()  # Set correct type
        self.account_history_blacklist_ops = Untouched()  # Set correct type
        self.history_blacklist_ops = Untouched()  # Set correct type
        self.history_disable_pruning = Untouched()  # Set correct type
        self.account_history_rocksdb_path = String()
        self.account_history_rocksdb_track_account_range = Untouched()  # Set correct type
        self.account_history_rocksdb_whitelist_ops = Untouched()  # Set correct type
        self.account_history_rocksdb_blacklist_ops = Untouched()  # Set correct type
        self.block_data_export_file = Untouched()  # Set correct type
        self.block_log_info_print_interval_seconds = Untouched()  # Set correct type
        self.block_log_info_print_irreversible = Untouched()  # Set correct type
        self.block_log_info_print_file = Untouched()  # Set correct type
        self.shared_file_dir = String()
        self.shared_file_size = Untouched()  # Set correct type
        self.shared_file_full_threshold = Untouched()  # Set correct type
        self.shared_file_scale_rate = Untouched()  # Set correct type
        self.checkpoint = Untouched()  # Set correct type
        self.flush_state_interval = Untouched()  # Set correct type
        self.cashout_logging_starting_block = Untouched()  # Set correct type
        self.cashout_logging_ending_block = Untouched()  # Set correct type
        self.cashout_logging_log_path_dir = Untouched()  # Set correct type
        self.debug_node_edit_script = Untouched()  # Set correct type
        self.edit_script = Untouched()  # Set correct type
        self.follow_max_feed_size = Untouched()  # Set correct type
        self.follow_start_feeds = Untouched()  # Set correct type
        self.log_json_rpc = Untouched()  # Set correct type
        self.market_history_bucket_size = Untouched()  # Set correct type
        self.market_history_buckets_per_size = Untouched()  # Set correct type
        self.p2p_endpoint = Untouched()
        self.p2p_max_connections = Untouched()  # Set correct type
        self.seed_node = Untouched()  # Set correct type
        self.p2p_seed_node = List(Untouched)
        self.p2p_parameters = Untouched()  # Set correct type
        self.rc_skip_reject_not_enough_rc = Untouched()  # Set correct type
        self.rc_compute_historical_rc = Untouched()  # Set correct type
        self.rc_start_at_block = Untouched()  # Set correct type
        self.rc_account_whitelist = Untouched()  # Set correct type
        self.snapshot_root_dir = String()
        self.statsd_endpoint = Untouched()  # Set correct type
        self.statsd_batchsize = Untouched()  # Set correct type
        self.statsd_whitelist = Untouched()  # Set correct type
        self.statsd_blacklist = Untouched()  # Set correct type
        self.tags_start_promoted = Untouched()  # Set correct type
        self.tags_skip_startup_update = Untouched()  # Set correct type
        self.transaction_status_block_depth = Untouched()  # Set correct type
        self.transaction_status_track_after_block = Untouched()  # Set correct type
        self.webserver_http_endpoint = Untouched()
        self.webserver_unix_endpoint = Untouched()
        self.webserver_ws_endpoint = Untouched()
        self.webserver_enable_permessage_deflate = Untouched()  # Set correct type
        self.rpc_endpoint = Untouched()  # Set correct type
        self.webserver_thread_pool_size = Untouched()  # Set correct type
        self.enable_stale_production = Boolean()
        self.required_participation = Integer()
        self.witness = List(String, single_line=False)
        self.private_key = List(Untouched, single_line=False)
        self.witness_skip_enforce_bandwidth = Untouched()  # Set correct type
        self.psql_url = Untouched()
        self.psql_index_threshold = Integer()
        self.psql_operations_threads_number = Integer()
        self.psql_transactions_threads_number = Integer()

    def __setattr__(self, key, value):
        entries = self.__get_entries()
        if self.__is_initialization_stage():
            entries[key] = value
        else:
            try:
                entries[key].set_value(value)
            except KeyError as error:
                raise KeyError(f'There is no such entry like \"{key}\"') from error

    def __getattr__(self, key):
        entries = self.__get_entries()
        try:
            return entries[key].get_value()
        except KeyError as error:
            raise KeyError(f'There is no such entry like \"{key}\"') from error

    def __get_entries(self):
        return self.__getattribute__('__entries')

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            raise Exception('Comparison with unsupported type')
        return not self.get_differences_between(other, stop_at_first_difference=True)

    def __ne__(self, other):
        return not self == other

    def get_differences_between(self, other, stop_at_first_difference=False):
        differences = {}
        names_of_entries = self.__get_entries().keys()
        for name_of_entry in names_of_entries:
            mine = getattr(self, name_of_entry)
            his = getattr(other, name_of_entry)

            if mine != his:
                differences[name_of_entry] = (mine, his)
                if stop_at_first_difference:
                    break

        return differences

    def write_to_lines(self):
        def should_skip_entry(entry):
            value = entry.get_value()
            if value is None:
                return True

            if isinstance(value, list) and not value:
                return True

            return False

        file_entries = []
        entries = self.__get_entries()
        for key, entry in entries.items():
            if should_skip_entry(entry):
                continue

            values = entry.serialize_to_text()
            values = values if isinstance(values, list) else [values]
            for value in values:
                file_entries.append(f"{key.replace('_', '-')} = {value}")

        return file_entries

    def write_to_file(self, file_path):
        self.validate()

        with open(file_path, 'w') as file:
            file.write('\n'.join(self.write_to_lines()))

    def validate(self):
        self.__assert_no_plugins_duplicates()
        self.__assert_valid_plugins_names()

    def __assert_no_plugins_duplicates(self):
        plugin_occurences = {plugin: 0 for plugin in self.plugin}
        for plugin in self.plugin:
            plugin_occurences[plugin] += 1

        duplicated_plugins = [plugin for plugin, occurences in plugin_occurences.items() if occurences > 1]
        if duplicated_plugins:
            raise RuntimeError(
                f'Following plugins are included more than once:\n'
                f'{duplicated_plugins}\n'
                f'\n'
                f'Remove places from code where you added them manually.'
            )

    def __assert_valid_plugins_names(self):
        for plugin in self.plugin:
            Plugin.validate(plugin)

    def load_from_lines(self, lines):
        assert isinstance(lines, list)

        def parse_entry_line(line):
            result = re.match(r'^\s*([\w\-]+)\s*=\s*(.*?)\s*$', line)
            return (result[1], result[2]) if result is not None else None

        def is_entry_line(line):
            return parse_entry_line(line) is not None

        self.__clear_values()
        for line in lines:
            if is_entry_line(line):
                key, value = parse_entry_line(line)

                self.__check_if_key_from_file_is_valid(key)

                if value != '':
                    entries = self.__get_entries()
                    entries[key.replace('-', '_')].parse_from_text(value)

    def __check_if_key_from_file_is_valid(self, key_to_check):
        """Keys from file have hyphens instead of underscores"""
        valid_keys = [key.replace('_', '-') for key in self.__get_entries().keys()]

        if key_to_check not in valid_keys:
            raise KeyError('Wrong config entry name')

    def __clear_values(self):
        entries = self.__get_entries()
        for entry in entries.values():
            entry.clear()

    def load_from_file(self, file_path):
        with open(file_path) as file:
            self.load_from_lines(file.readlines())
