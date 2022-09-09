### Changes in Hived command-line options:

#### Application Options:
*New:*
- `--block-data-skip-empty arg (=0)` - skip producing when no factory is registered,
- `--enable-block-log-compression arg (=1)` - compress blocks using zstd as they're added to the block log,
- `--block-log-compression-level arg (=15)` - block log zstd compression level 0 (fast, low compression) - 22 (slow, high compression),
- `--blockchain-thread-pool-size size (=8)` - Number of worker threads used to pre-validate transactions and blocks,
- `--block-stats-report-type arg (=FULL)` - level of detail of block stat reports: NONE, MINIMAL, REGULAR, FULL. Default FULL (recommended for API nodes),
- `--block-stats-report-output arg (=ILOG)` - where to put block stat reports: DLOG, ILOG, NOTIFY. Default ILOG,
- `--notifications-endpoint arg` - list of addresses, that will receive notification about in-chain events.

*Replaced:*
The `account_history` plugin has been removed and replaced with the `rocksdb` plugin.

| action   | account_history [old]                       | rocksdb [new]                                       | desc                                                                                                        |
|----------|---------------------------------------------|-----------------------------------------------------|-------------------------------------------------------------------------------------------------------------|
| replaced | `--account-history-track-account-range arg` | `--account-history-rocksdb-track-account-range arg` | defines a range of accounts to track as a json pair ["from","to"] [from,to] Can be specified multiple times |
| replaced | `--account-history-whitelist-ops arg`       | `--account-history-rocksdb-whitelist-ops arg`       | defines a list of operations which will be explicitly logged                                                |
| replaced | `--account-history-blacklist-ops arg`       | `--account-history-rocksdb-blacklist-ops arg`       | defines a list of operations which will be explicitly ignored                                               |
| deleted  | `--history-disable-pruning arg (=0)`        |                                                     | disables automatic account history trimming                                                                 |
 

*Syntax changes:*
- `--shared-file-dir arg (="blockchain")` -> `--shared-file-dir dir (="blockchain")` - the location of the chain shared memory files (absolute path or relative to application data dir),
- `--shared-file-size arg (=54G)` -> `--shared-file-size arg (=24G)` - size of the shared memory file. Default: 24G (previous 54G). If running with many plugins, increase this value to 28G,
- `--log-appender arg (={"appender":"stderr","stream":"std_error"} {"appender":"p2p","file":"logs/p2p/p2p.log"})` -> `--log-appender arg (={"appender":"stderr","stream":"std_error","time_format":"iso_8601_microseconds"} {"appender":"p2p","file":"logs/p2p/p2p.log","time_format":"iso_8601_milliseconds", "delta_times": false})` - appender definition json: {"appender", "stream", "file"} Can only specify a file OR a stream.


#### Application Config Options
- `--plugin arg` -> `--plugin plugin-name` - plugin(s) to enable, may be specified 


#### Application Command Line Options:

*New:*
- `--list-plugins` - print names of all available plugins and exit,
- `--generate-completions` - generate bash auto-complete script (try: eval "$(hived --generate-completions)"),
- `--exit-before-sync ` - exits before starting sync, handy for dumping snapshot without starting replay,
- `--validate-during-replay` - runs all validations that are normally turned off during replay,
- `--skeleton-key arg (=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n)` - WIF PRIVATE key to be used as skeleton key for all accounts. Only available for mirror net build.

*Deleted:*
- `--account-history-rocksdb-immediate-import` - allows to force immediate data import at plugin startup. By default storage is supplied during reindex process.
- `--rc-compute-historical-rc` - generate historical resource credits

*Syntax changes:*
- `-d [ --data-dir ] arg` -> `-d [ --data-dir ] dir` - directory containing configuration file config.ini. Default location: $HOME/.hived or CWD/.hived
- `-c [ --config ] arg (="config.ini")` -> `-c [ --config ] filename (="config.ini")` - configuration file name relative to data-dir
