#!/usr/bin/python3

# Easier way to generate configs


class config:
    def __init__(self, check_is_arg_exists: bool = True, **kwargs):
        self.log_appender = '{"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"}'
        self.log_logger = (
            '{"name":"default","level":"all","appender":"stderr"} {"name":"p2p","level":"all","appender":"p2p"}'
        )
        self.backtrace = "yes"
        self.plugin = "witness database_api account_by_key_api network_broadcast_api condenser_api block_api transaction_status_api debug_node_api"
        self.account_history_rocksdb_path = '"blockchain/account-history-rocksdb-storage"'
        self.block_data_export_file = "NONE"
        self.block_data_skip_empty = "0"
        self.block_log_info_print_interval_seconds = "86400"
        self.block_log_info_print_irreversible = "1"
        self.block_log_info_print_file = "ILOG"
        self.shared_file_dir = '"blockchain"'
        self.shared_file_size = "8G"
        self.shared_file_full_threshold = "0"
        self.shared_file_scale_rate = "0"
        self.follow_max_feed_size = "500"
        self.follow_start_feeds = "0"
        self.market_history_bucket_size = "[15,60,300,3600,86400]"
        self.market_history_buckets_per_size = "5760"
        self.p2p_seed_node = "127.0.0.1:2001"
        self.rc_skip_reject_not_enough_rc = "0"
        self.statsd_batchsize = "1"
        self.tags_start_promoted = "0"
        self.tags_skip_startup_update = "0"
        self.transaction_status_block_depth = "64000"
        self.transaction_status_track_after_block = "0"
        self.webserver_http_endpoint = "127.0.0.1:8090"
        self.webserver_ws_endpoint = "127.0.0.1:8090"
        self.webserver_thread_pool_size = "32"
        self.enable_stale_production = "0"
        self.required_participation = "0"
        self.witness = '"initminer"'
        self.private_key = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"
        self.witness_skip_enforce_bandwidth = "1"
        self.snapshot_root_dir = '"/tmp/snapshots"'

        for key, value in kwargs.items():
            assert (not check_is_arg_exists) or key in self.__dict__.keys(), f"key `{key}` not found"
            setattr(self, key, value)

    def generate(self, path_to_file: str):
        conf = self.__dict__
        with open(path_to_file, "w") as file:
            for key, value in conf.items():
                if value is not None:
                    file.write("{} = {}\n".format(key.replace("_", "-"), value))

    def load(self, path_to_file: str):
        _keys = list(self.__dict__.keys())
        keys = []
        for key in _keys:
            setattr(self, key, None)
            keys.append(key.replace("_", "-"))

        def proc_line(line: str):
            values = line.split("=")
            return values[0].strip("\n\r "), values[1].strip("\n\r ")

        def match_property(line: str):
            line = line.strip(" \n\r")
            if line.startswith("#") or len(line) == 0:
                return
            key, value = proc_line(line)
            if key in keys:
                setattr(self, key.replace("-", "_"), value)

        with open(path_to_file, "r") as file:
            for line in file:
                match_property(line)

    def update_plugins(self, required_plugins: list):
        plugins = self.plugin.split(" ")
        plugins.extend(x for x in required_plugins if x not in plugins)
        self.plugin = " ".join(plugins)

    def exclude_plugins(self, plugins_to_exclude: list):
        current_plugins = self.plugin.split(" ")
        for plugin in plugins_to_exclude:
            try:
                current_plugins.remove(plugin)
            except ValueError:
                pass
        self.plugin = " ".join(current_plugins)


def validate_address(val: str) -> bool:
    try:
        val = val.strip()
        address, port = val.split(":")
        port = int(port)

        assert port >= 0
        assert port < 0xFFFF

        def validate_ip(ip: list):
            addr = ip.split(".")
            assert len(ip) == 4
            for part in addr:
                x = int(part)
                assert x >= 0
                assert x <= 255

        try:
            validate_address(address)
        except:
            from socket import gethostbyname as get_ip

            validate_address(get_ip(address))

        return True

    except:
        return False
