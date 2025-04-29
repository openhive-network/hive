# Blockchain converter
Blockchain converter is a helper conversion tool.

## Goal
Blockchain converter lets you add a custom second authority and sign blocks even without having an account on the Hive blockchain after replaying your custom node with a converted block log.

## Building
To build the blockchain converter you will have to enable the `HIVE_CONVERTER_BUILD` option in CMake.
If you build this project without specifying the converter flag it will dev-warn you about it.

Note: This tool is also intended to be used only in the mainnet build.

## Configuration
For the program to work properly you will have to specify the input and output sources using `input` and `output` options. It can be either a file or URL(s) (it is plugin-defined. See [Plugins](#Plugins)).

You will also have to specify the `chain-id` and `private-key` options, where the private key is your block signing key (witness private key) and the chain ID is your altered chain ID.

### Second authority
You can optionally specify the `owner`, `active`, and `posting` second authority private keys. They will be auto-generated, if not provided.

Note: If you use the `use-same-key` option, the second authority keys will be the same as the one, provided using the `private-key` option.

All of the second authority keys will be printed before exiting the program.

### Plugins
You can choose a conversion method by using the `plugin` option.

#### `block_log_conversion`
The `block_log_conversion` plugin relies on the `block_log` files.
As the `input` option value you will have to specify a path to a non-empty input original block_log file and as the `output` option value you should specify a path to an output `block_log` file that will contain converted blockchain.

#### `node_based_conversion`
The `node_based_conversion` plugin relies on the remote nodes.
As the `input` option value you will have to specify a URL to the original input mainnet node (make sure that the given node uses block API and database API) and as the `output` option value you should specify URLs to the nodes with altered chain (make sure that given nodes use condenser API).

In favor of the old `use-now-time` option, there is a new feature that calculates transaction expiration times based on the time gap between the output node
head block time and the timestamp of the block that is currently being converted. There is also a transaction expiration check that should perfectly work for the majority of converted transactions (formula):
```c++
// Use the deduced transaction expiration value, unless it would be too far in the future to be allowed, then use the largest future time
min(
  // Try to match the relative expiration time of the original transaction to the block it was held in, unless it would be so short that the transaction would likely expire. After more thought, I'm starting to question the logic of this, maybe we would prefer if these transactions don't expire if at all possible (after all, they got into mainnet). But let's leave this as-is for now.
  max( block_to_convert.timestamp + trx_time_offset, transaction_to_convert.expiration + trx_time_offset),
  // Subtract `(trx_time_offset - block_offset)` to avoid trx id duplication (we assume that there should not be more than 3600 txs in the block)
  now_time + (HIVE_MAX_TIME_UNTIL_EXPIRATION-HIVE_BC_SAFETY_TIME_GAP) - (trx_time_offset - block_offset)
)
```

where:
* `block_offset` is the mentioned time gap without the safety "nonce"
* `trx_time_offset` is the mentioned time gap plus the safety "nonce" for the transactions to avoid their ID duplication
* `HIVE_BLOCK_INTERVAL` is the standard hive block production interval which (in the standard configuration) is 3 seconds
* `HIVE_MAX_TIME_UNTIL_EXPIRATION` is the maximum time until the transaction expiration which (in the standard configuration) is 1 hour
* `HIVE_BC_SAFETY_TIME_GAP` is made especially for the `node_based_conversion` plugin in which the head block time is deduced (it in the standard configuration equals 30 seconds)


This plugin retrieves blocks from the remote and holds them in the block buffer. You can specify the block buffer size with the `block-buffer-size` option, which defaults to 1000.

Notes:
* URL should have the format: `http://ip-or-host:port`. If you want to use the default HTTP port use `80`.
* This conversion tool currently does not support the `https` protocol and chunked transfer encoding in API node responses
* The last irreversible block for the [TaPoS](https://github.com/EOSIO/Documentation/blob/master/TechnicalWhitePaper.md#transaction-as-proof-of-stake-tapos) generation is always taken from the first of the specified output nodes

#### `iceberg_generate`
`iceberg_generate` works just like [`block_log_conversion`](#block_log_conversion) plugin, but converts only a given range of blocks, generating required information for converted operations to work, like creating non-existing accounts, posts (comments), etc.

There are 3 stages of the iceberg conversion:
1. Preparing blockchain for the conversion:
  - Waiting for more than 1 witness to appear in the blockchain
  - Updating properties of the witnesses to maximalize generated block size and set the lowest possible account creation fee
  - Waiting for the properties to be applied
2. Collector stage:
  - Collecting dependent accounts and comments and creating them
  - Testing if there is enough supply
3. Conversion stage:
  - Converts blocks and transmits them to the output node

Notes:
- Transaction expiration time is always set to half of the maximum expiration time, instead of the formula mentioned in the [`node_based_conversion`](#node_based_conversion)
- Iceberg conversion requires creating treasury accounts and forcing some internal `hived` logic. That is why **blockchain converter with `iceberg generate` plugin MUST be configured under cmake using the option `-DHIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED=ON`**

### Multithreading support
Signing transactions takes a relatively large amount of time, so this tool enables multithreading support.
By default, it uses only 1 signing thread. If you want to increase this value, use the `jobs` option and specify the number of signing threads.

### Stopping and resuming the conversion
If you want to stop the conversion at a specific block, you will have to provide the `stop-block` option.

If you want to resume it later, the converter will automatically retrieve the last block number if either the `resume-block` option has not been specified or its value is not equal to 0.
If use [block_log_conversion plugin](#block_log_conversion) this value will be set to the last block number in the output block log + 1 and if you use [node_based_conversion plugin](#node_based_conversion) it will be equal to the last `head_block_number` value retrieved from the `database_api.get_dynamic_global_properties` API call sent to the output node + 1.

Note: If you stop the conversion before hardfork 17, the converter state will be most probably corrupted and you will be unable to resume the conversion from the given state. To continue, you will have to restart the conversion from the beginning.

Another note: [`iceberg_generate`](#iceberg_generate) plugin does not allow resuming the conversion due to the required reference operations state that is not being saved on program interrupt

### Logging
You can log block information before and after each conversion.

To log every n blocks use the `log-per-block` option or log only specific blocks using the `log-specific` option.

## Configuring **live** conversion
If you have successfully replayed your converted blockchain as described in [Configuring altered P2P network](#Configuring%20altered%20P2P%20network), you can now run a live peer-to-peer network using the previously mentioned [node_based_conversion plugin](#node_based_conversion%20plugin)

Our goal is to run one node with the altered chain in live sync, signing and producing blocks along with the blockchain converter with the [node_based_conversion plugin](#node_based_conversion%20plugin) enabled.

Assuming that you are using one of the previously configured nodes, for example, `alice-hived`, do the following:

Alter the configuration file by adding the HTTP server and required APIs (`alice-data/config.ini`):
```ini
# http server
webserver-http-endpoint = localhost:8800

# required APIs for the blockchain converter
plugin = block_api database_api network_broadcast_api
```
Then run the node:
```bash
alice-hived -d alice-data --chain-id 1
```

Just after running the first node, configure the second one (Bob) with the P2P endpoint and seeds. You should also add the rest of the witnesses:
```ini
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
```bash
bob-hived -d bob-data --chain-id 1
```

After running the second node, stop the first one, open its configuration (Alice configuration), and add a seed node:
```ini
# Bob node:
p2p-seed-node=127.0.0.1:40402
```
Re-run the node (Alice):
```bash
alice-hived -d alice-data --chain-id 1
```

Now your nodes should be able to communicate with each other and produce blocks.

You can also now disable the stale production and increase the required participation in the configuration files.

### Skeleton key
For all the validations skipped during the replay to work, we should own the `initminer` private key to produce blocks. This option was previously hardcoded and available only in the testnet build, but along with the converter development, you can now specify your own "skeleton key" that serves as a unique `initminer` private key and witness key. This option is available **only in `hived`** built as an alternate chain (mirrornet or testnet). This option is mandatory when replaying with the `validate-during-replay` option enabled or in the live sync.

## Configuring altered P2P network
If you have successfully converted your blockchain using one of the previously mentioned [conversion examples](#Example%20run), you can now create your altered peer-to-peer network.

Our goal is to run two separate nodes that will see each other in the peer-to-peer network, producing and signing new blocks using the main 21 witnesses while simultaneously exchanging information with each other.

For that purpose, we will build and configure two nodes (remember to enable the `HIVE_CONVERTER_BUILD` option while running CMake. Otherwise you will be not able to change the chain ID):
* `alice-hived` with the configuration directory at `alice-data`
* `bob-hived` with the configuration directory at `bob-data`

First, you will have to replay nodes using the same altered `block_log` file (copy the converted block log to the `data/blockchain` directory).
Also, we do not want them to synchronize with the network yet, so we will have to use the `exit-before-sync` flag:
```bash
alice-hived -d alice-data --replay-blockchain --exit-before-sync --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
```bash
bob-hived -d alice-data --replay-blockchain --exit-before-sync --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```
(Do not forget about the [skeleton key](#Skeleton%20key)!)

Remember the last irreversible block number.

Now retrieve the main 21 witnesses. You can do that by - for example - iterating through the last 21 blocks starting from the last irreversible block (you can find this data on e.g. [hiveblocks.com](hiveblocks.com)).

Then apply the proper config in your configuration file for the first node (add just one witness) (for example, it will be `gtg`):
```ini
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
```bash
alice-hived -d alice-data --chain-id 1 --skeleton-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
```

After that run the blockchain converter:
```bash
blockchain_converter --plugin node_based_conversion --input 'http://api.deathwing.me:80' --output 'http://127.0.0.1:80' --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
As the input node, we are using `https://api.deathwing.me:80`. You can use any fully-replayed mainnet node that has an HTTP webserver.

It is also recommended to set the `-R` option, which explicitly tells the converter from which mainnet block it should resume the conversion process.

Now your converter will infinitely send the transactions to the output node. After reaching the head mainnet block, it will check for the new block every 1 second.

## Configuring **iceberg** node

Assuming that the hived `data` directory is located under: `$(pwd)/data`:

[`iceberg_generate`](#iceberg_generate) plugin may require adding alternate chain specification. You can use this helper script to create the required 20 + `initminer` (21 in total) witnesses, and also alter some of the hive definitions:

```bash
#!/bin/bash

cat << EOT > data/alternate-chain-spec.json
{
  "init_supply": 1000000000000,
  "hbd_init_supply": 1000000000000,
  "genesis_time": $(date +%s),
  "hardfork_schedule": [
    {"hardfork": 28, "block_num": 1}
  ],
  "min_root_comment_interval": 0,
  "min_reply_interval": 0,
  "min_comment_edit_interval": 0,
  "custom_op_block_limit": 10000000,
  "init_witnesses": [
    "uknwn.wit01", "uknwn.wit02", "uknwn.wit03", "uknwn.wit04", "uknwn.wit05", "uknwn.wit06"
    "uknwn.wit07", "uknwn.wit08", "uknwn.wit09", "uknwn.wit10", "uknwn.wit11", "uknwn.wit12",
    "uknwn.wit13", "uknwn.wit14", "uknwn.wit15", "uknwn.wit16", "unkwn.wit17", "uknwn.wit18",
    "uknwn.wit19", "uknwn.wit20"
  ]
}
EOT

./hived -d data --chain-id 9372 --skeleton-key=5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --alternate-chain-spec data/alternate-chain-spec.json
```
The given configuration sets the initial supply, comment options, and genesis time, and ensures that HF28 will be applied in the genesis block. You can specify your hardfork schedule if you are converting a different range of blocks (like HF1 for range `906000` - `907000`).

After creating and configuring the script you can run the P2P network in a very similar way to how it is presented in [Configuring altered P2P network](#configuring-altered-p2p-network). Remember to add given plugins to the configuration of either the first output node or all of them:
```ini
plugin = account_by_key account_by_key_api transaction_status_api
# Default value for transaction status block depth is 64000 and
# Since we are using transaction_status_api only to test if the initial conversion stage is complete,
# Setting this option to 0 is recommended if you do not want to allow an increase of the `transaction_status_object` index (shared memory file size also increases)
transaction-status-block-depth = 0
```

After that, your hived network is ready to be used with the [`iceberg_generate`](#iceberg_generate) plugin

### node_based_conversion debug definitions
If the node_based_conversion plugin encounters any error after sending the convert transaction to the output node, it will print the short message.
You can disable this by defining the `HIVE_CONVERTER_POST_SUPPRESS_WARNINGS` identifier

You can also enable additional debug info by defining the `HIVE_CONVERTER_POST_DETAILED_LOGGING` identifier

`HIVE_CONVERTER_POST_COUNT_ERRORS` - counts errors and their types and displays them at the program shutdown

## Example run

### block_log_conversion plugin

#### Block log Use case

Transpiles source mainnet block_log data from genesis up to a given block (or entire set of data) to the specified target block_log data called mirrornet. (No node usage required)

#### Block log Description

Convert the `block_log` file and dump the output to the `new_fancy_block_log` file, alter chain to use the id `1`, sign blocks using witness private key: `5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n`. This key will be also used as a second authority key for every user:
```bash
blockchain_converter --plugin block_log_conversion --input block_log --output new_fancy_block_log --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
Note: block_log_conversion plugin increases the block size by default to the `HIVE_MAX_BLOCK_SIZE` (2 MiB) using the `witness_update` and `witness_set_properties` operations. After a replay, you can change this using your witness and run node_based_conversion normally without the increase in block size

### node_based_conversion plugin

#### Node based Use case

Transpiles source mainnet block_log data from a given range to the specified target node running on the previously transpiled using [block_log_conversion](#block_log_conversion%20plugin) data. (Requires at least one node for signing)

You can use it to resume the conversion in so-called "live mode" using the existing hive mainnet network, from which the data will be pulled, converted, and pushed to your configured signing node. Output block log will be produced by the node and no local block_log inputs/outputs are required.

#### Node based Description

Convert blocks from `api.deathwing.me` and send converted transactions to the `127.0.0.1` node with the same private keys and chain ID as in the [block_log_conversion plugin example run](#block_log_conversion%20plugin)
```
blockchain_converter --plugin node_based_conversion --input 'http://api.deathwing.me:80' --output 'http://127.0.0.1:80' --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```

### iceberg_generate plugin

#### Iceberg Use case

Iceberg is a standalone plugin and does not depend on anything like for example [node_based_conversion](#node_based_conversion%20plugin) depends on [block_log_conversion](#block_log_conversion%20plugin). The main purpose of this tool is to generate a valid blockchain from a range of blocks, like the last 5 million blocks.

As mentioned previously it has multiple stages and generates all of the required data for the transactions to exist. The acceptable error margin is about a 15% loss in existing transactions.

You will need an existing source mainnet block_log data and configured output node for signing (similarly as it was done in the [node_based_conversion](#node_based_conversion%20plugin))

#### Iceberg Description

Convert blocks in 100000-200000 range from the `block_log` file and dump the output to `http://127.0.0.1:8081` node, alter chain to use the id `1`, sign blocks using witness private key: `5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n`. This key will be also used as a second authority key for every user:
```bash
blockchain_converter --plugin iceberg_generate --resume-block 100000 --stop-block 200000 --input block_log --output http://127.0.0.1:8081 --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
Note: Block size will be increased just like in the [`block_log_conversion` plugin example run](#block_log_conversion-plugin). Also, additional operations will be added (See [`iceberg_generate`](#iceberg_generate)).
