### Changes in Hived config.ini file:

*New:*

- `enable-block-log-compression = 1` - compress blocks using zstd as they're added to the block log,

- `block-log-compression-level = 15` - block log zstd compression level 0 (fast, low compression) - 22 (slow, high compression),

- `blockchain-thread-pool-size = 8` - number of worker threads used to pre-validate transactions and blocks,

- `block-stats-report-type = FULL`, level of detail of block stat reports: NONE, MINIMAL, REGULAR, FULL. Default FULL (recommended for API nodes),

- `block-stats-report-output = ILOG` - where to put block stat reports: DLOG, ILOG, NOTIFY. Default ILOG.

*Deleted:*

- `account-history-track-account-range` - defines a range of accounts to track as a json pair ["from","to"] [from,to] Can be specified multiple times,
  
- `track-account-range` - defines a range of accounts to track as a json pair ["from","to"] [from,to] Can be specified multiple times. Deprecated in favor of `account-history-track-account-range`,

- `account-history-whitelist-ops` - defines a list of operations which will be explicitly logged,

- `history-whitelist-ops` - defines a list of operations which will be explicitly logged. Deprecated in favor of `account-history-whitelist-ops`,

- `account-history-blacklist-ops` - defines a list of operations which will be explicitly ignored,

- `history-blacklist-ops` - defines a list of operations which will be explicitly ignored. Deprecated in favor of `account-history-blacklist-ops`,
  
- `history-disable-pruning = 0` - disables automatic account history trimming,

- `rc-compute-historical-rc = 0` - generate historical resource credits.

*Changes:*

- Size of the shared memory file. Changed default value to 24G,

- `log-appender` - additional log capabilities. Has a new bool field `delta_times`. By default, set to False.

- P2P network parameters - value of the field `maximum_number_of_sync_blocks_to_prefetch` has been changed from `2_000` to `20_000`. The increase was possible through to the optimization of the P2P layer.
