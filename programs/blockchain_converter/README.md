# Blockchain converter
Blockchain converter is a helper conversion tool.

## Goal
Blockchain converter lets you add a custom second authority and sign blocks even without having an account on the Hive blockchain after replaying your custom node with a converted block log.

## Building
To build the blockchain converter you will have to enable the `HIVE_CONVERTER_BUILD` option in CMake.
If you build this project without specifying the converter flag it will dev-warn you about it.

Note: This tool is also intended to be used only in the mainnet build.

## Configuration
For the program to work properly you will have to specify the input and output sources using `input` and `output` options. It can be either file or URL(s) (it is plugin-defined. See [Plugins](#Plugins)).

You will also have to specify the `chain-id` and `private-key` options, where the private key is your block signing key (witness private key) and the chain id is your altered chain id.

### Second authority
You can optionally specify the `owner`, `active` and `posting` second authority private keys. They will be auto-generated, if not provided.

Note: If you use the `use-same-key` option, second authority keys will be the same as the one, provided using the `private-key` option.

All of the second authority keys will be printed before exiting the program.

### Plugins
You can choose a conversion method by using the `plugin` option.

#### `blog_log_conversion`
`block_log_conversion` plugin relies on the `block_log` files.
As the `input` option value you will have to specify a path to a non-empty input original block_log file and as the `output` option value you should specify a path to an output `block_log` file that will contain converted blockchain.

#### `node_based_conversion`
`node_based_conversion` plugin relies on the remote nodes.
As the `input` option value you will have to specify an URL to the original input mainnet node (make sure that given node uses block API and database API) and as the `output` option value you should specify URLs to the nodes with altered chain (make sure that given nodes use condenser API).

In favor of the old `use-now-time` option there is a new feature that calculates transaction expiration times based on the time gap between output node
head block time and the timestamp of the block that is currently being converted. There is also a transaction expiration check that should perfectly work for the majority of converted transactions (formula):
```c++
// Use the deduced transaction expiration value, unless it would be too far in the future to be allowed, then use largest future time
min(
  // Try to match relative expiration time of original transaction to including block, unless it would be so short that transaction would likely expire. After more thought, I'm starting to question the logic on this, maybe we would prefer if these transactions don't expire if at all possible (after all, they got into mainnet). But let's leave this as-is for now.
  max( block_to_convert.timestamp + trx_time_offset - HIVE_BLOCK_INTERVAL, transaction_to_convert.expiration + trx_time_offset),
  now_time - HIVE_BLOCK_INTERVAL + HIVE_MAX_TIME_UNTIL_EXPIRATION
)
```

where:
* `trx_time_offset` is the mentioned time gap plus the safety "nonce" for the transactions to avoid their ids duplication
* `HIVE_BLOCK_INTERVAL` is the standard hive block production interval which (in the standard configuration) is 3 seconds
* `HIVE_MAX_TIME_UNTIL_EXPIRATION` is the maximum time until the transaction expiration which (in the standard configuration) is 1 hour


This plugin retrieves blocks from the remote and holds them in the block buffer. You can specify the block buffer size with the `block-buffer-size` option, which defaults to 1000.

Notes:
* URL should have the format: `http://ip-or-host:port`. If you want to use the default HTTP port use `80`.
* This conversion tool currently does not support `https` protocol and chunked transfer encoding in API node responses
* Last irreversible block for the [TaPoS](https://github.com/EOSIO/Documentation/blob/master/TechnicalWhitePaper.md#transaction-as-proof-of-stake-tapos) generation is always taken from the first of the specified output nodes

### Multithreading support
Signing transactions takes a relatively large amount of time, so this tool enables multithreading support.
By default, it uses only 1 signing thread. If you want to increase this value, use the `jobs` option and specify the number of signing threads.

### Stopping and resuming the conversion
If you want to stop the conversion at a specific block, you will have to provide the `stop-block` option.

If you want to resume it later, the converter will automatically retrieve the last block number if either the `resume-block` option has not been specified or its value is not equal to 0.
If use [blog_log_conversion plugin](#blog_log_conversion) this value will be set to the last block number in the output block log + 1 and if you use [node_based_conversion plugin](#node_based_conversion) it will be equal to the last `head_block_number` value retrieved from the `database_api.get_dynamic_global_properties` API call sent to the output node + 1.

Note: If you stop the conversion before hardfork 17, the converter state will be most probably corrupted and you will be unable to resume the conversion from the given state. To continue, you will have to restart the conversion from the beginning.

### Logging
You can log block information before and after each conversion.

To log every n blocks use the `log-per-block` option or log only specific blocks using the `log-specific` option.

## Example run
#### block_log_conversion plugin
Convert `block_log` file and dump the output to `new_fancy_block_log` file, alter chain to use the id `1`, sign blocks using witness private key: `5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n`. This key will be also used as a second authority key for every user:
```
blockchain_converter --plugin block_log_conversion --input block_log --output new_fancy_block_log --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
Note: block_log_conversion plugin increases the block size by default to the `HIVE_SOFT_MAX_BLOCK_SIZE` (2 MiB) using the `witness_update` and `witness_set_properties` operations. After replay you can change this using your witness and run node_based_conversion normally without the increase in blocks size

#### node_based_conversion plugin
Convert blocks from `api.deathwing.me` and send converted transactions to `127.0.0.1` node with same private keys and chain id as in the [block_log_conversion plugin example run](#block_log_conversion%20plugin)
```
blockchain_converter --plugin node_based_conversion --input 'http://api.deathwing.me:80' --output 'http://127.0.0.1:80' --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```

## Configuring **live** conversion
If you have successfully replayed your converted blockchain as described in [Configuring altered P2P network](#Configuring%20altered%20P2P%20network), you can now run a live peer-to-peer network using previously mentioned [node_based_conversion plugin](#node_based_conversion%20plugin)

Our goal is to run one node with the altered chain in live sync, signing and producing blocks along with the blockchain converter with the [node_based_conversion plugin](#node_based_conversion%20plugin) enabled.

Assuming that you are using one of the previously configured nodes, for example `alice-hived`, do the following:

Alter the configuration file by adding the http server and required APIs (`alice-data/config.ini`):
```
# http server
webserver-http-endpoint = localhost:8800

# required APIs for the blockchain converter
plugin = block_api database_api network_broadcast_api
```
Then run the node:
```
alice-hived -d alice-data --chain-id 1
```

Just after running the first node, configure the second one (Bob) with P2P endpoint and seeds. You should also add the rest of the witnesses:
```
# Enable the `witness` plugin
plugin = witness

# Run peer-to-peer endpoint on the node and configure the seeds
p2p-endpoint = 0.0.0.0:40402
# Alice node:
p2p-seed-node=127.0.0.1:40401

# Do not require any participation
required-participation = 0

# We should first enable the stale production
enable-stale-production = true

# main witness
witness="blocktrades"
# ... (witnesses)
witness="good-karma"

# witness signing key
private-key=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
Then run this node:
```
bob-hived -d bob-data --chain-id 1
```

After running the second node, stop the first one, open its configuration (Alice configuration) and add a seed node:
```
# Bob node:
p2p-seed-node=127.0.0.1:40402
```
Re-run the node (Alice):
```
alice-hived -d alice-data --chain-id 1
```

Now your nodes should be able to communicate with each other and produce blocks.

You can also now disable the stale production and increase the required participation in the configuration files.

### Skeleton key
For all the validations skipped during the replay to work, we should own the `initminer` private key to produce blocks. This option was previously hardcoded and available only in testnet build, but along with the converter development, you can now specify your own "skeleton key" that serves as a unique `initminer` private key and witness key. This option is available **only in `hived`** built as alternate chain (mirrornet or testnet). This option is mandatory when replaying with the `validate-during-replay` option enabled or in the live sync.

## Configuring altered P2P network
If you have successfully converted your blockchain using one of the previously mentioned [conversion examples](#Example%20run), you can now create your altered peer-to-peer network.

Our goal is to run two separate nodes that will see each other in the peer-to-peer network, producing and signing new blocks using the main 21 witnesses while simultaneously exchanging the information between each other.

For that purpose, we will build and configure two nodes (remember to enable the `HIVE_CONVERTER_BUILD` option while running CMake. Otherwise you will be not able to change the chain id):
* `alice-hived` with the configuration directory at `alice-data`
* `bob-hived` with the configuration directory at `bob-data`

First, you will have to replay nodes using the same altered `block_log` file (copy the converted block log to the `data/blockchain` directory).
Also, we do not want them to synchronize with the network yet, so we will have to use the `exit-before-sync` flag:
```
alice-hived -d alice-data --replay-blockchain --exit-before-sync --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
```
bob-hived -d alice-data --replay-blockchain --exit-before-sync --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
(Do not forget about the [skeleton key](#Skeleton%20key)!)

Remember the last irreversible block number.

Now retrieve the main 21 witnesses. You can do that by - for example - iterating through the last 21 blocks starting from the last irreversible block (you can find this data on e.g. [hiveblocks.com](hiveblocks.com)).

Then apply the proper config in your configuration file for the first node (add just one witness) (for example, it will be `gtg`):
```
# Enable the `witness` plugin
plugin = witness

# Run peer-to-peer endpoint on the node
p2p-endpoint = 0.0.0.0:40401

# Do not require any participation
required-participation = 0

# We should first enable the stale production
enable-stale-production = true

# main witness
witness="gtg"

# witness signing key
private-key=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
Then you should run the first node (Alice):
```
alice-hived -d alice-data --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```

After that run the blockchain converter:
```
blockchain_converter --plugin node_based_conversion --input 'http://api.deathwing.me:80' --output 'http://127.0.0.1:80' --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
As the input node we are using `https://api.deathwing.me:80`. You can use any fully-replayed mainnet node that has an HTTP webserver.

It is also recommended to set the `-R` option, which explicitly tells the converter from which mainnet block it should resume the conversion process.

Now your converter will infinitely send the transactions to the output node. After reaching the head mainnet block, it will check for the new block every 1 second.

### node_based_conversion debug definitions
If the node_based_conversion plugin encounters any error after sending the convert transaction to the output node, it will print the short message.
You can disable this by defining the `HIVE_CONVERTER_TRANSMIT_SUPPRESS_WARNINGS` identifier

You can also enable additional debug info by defining the `HIVE_CONVERTER_TRANSMIT_DETAILED_LOGGING` identifier
