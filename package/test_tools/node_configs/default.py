from test_tools.node_config import NodeConfig


def create_default_config():
    config = NodeConfig()

    config.log_appender = '{"appender":"stderr","stream":"std_error","time_format":"iso_8601_microseconds"} {"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"iso_8601_milliseconds"}'
    config.log_logger = '{"name":"default","level":"info","appender":"stderr"} '\
                        '{"name":"user","level":"debug","appender":"stderr"} '\
                        '{"name":"p2p","level":"warn","appender":"p2p"}'
    config.backtrace = 'yes'
    config.plugin = ['witness', 'account_by_key', 'account_by_key_api', 'wallet_bridge_api']
    config.history_disable_pruning = '0'
    config.account_history_rocksdb_path = 'blockchain/account-history-rocksdb-storage'
    config.block_data_export_file = 'NONE'
    config.block_data_skip_empty = False
    config.block_log_info_print_interval_seconds = '86400'
    config.block_log_info_print_irreversible = '1'
    config.block_log_info_print_file = 'ILOG'
    config.shared_file_dir = 'blockchain'
    config.shared_file_size = '54G'
    config.shared_file_full_threshold = '0'
    config.shared_file_scale_rate = '0'
    config.follow_max_feed_size = '500'
    config.follow_start_feeds = '0'
    config.market_history_bucket_size = '[15,60,300,3600,86400]'
    config.market_history_buckets_per_size = '5760'
    config.rc_skip_reject_not_enough_rc = '0'
    config.rc_compute_historical_rc = '0'
    config.rc_start_at_block = '0'
    config.snapshot_root_dir = 'snapshot'
    config.statsd_batchsize = '1'
    config.tags_start_promoted = '0'
    config.tags_skip_startup_update = '0'
    config.transaction_status_block_depth = '64000'
    config.transaction_status_track_after_block = '0'
    config.webserver_thread_pool_size = '32'
    config.enable_stale_production = False
    config.required_participation = 33
    config.witness_skip_enforce_bandwidth = '1'

    return config
