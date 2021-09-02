# Blockchain converter
Blockchain converter is a helper conversion tool.

## Goal
Blockchain converter lets you add a custom second authority and sign blocks even without having an account on hive blockchain after replaying hive node with a converted block log

## Building
In order to build blockchain converter you have to enable the `HIVE_CONVERTER_BUILD` option in cmake.
If you build this project without specifying the converter flag it will dev-warn you about it.

Note: This tool is also intended to be used only in mainnet build.

## Configuration
In order for program to work properly you have to specify the input and output sources using `input` and `output` options. It can be either file or url (it is plugin-defined. See [Plugins](#Plugins)).

You will also have to specify the `chain-id` and `private-key` option, where private key is your block signing key and chain id is your altered chain id to be used when signing transactions.

### Second authority
You can optionally specify the `owner`, `active` and `posting` second authority private keys. If you will not provide them, they will be auto generated.

Note: If you use `use-same-key` option, second authority keys will default to the provided `private-key`. If you are not sure which keys are being used as a second authority keys, they will be printed after the conversion.

### Plugins
You can choose plugin by passing `plugin` option to the converter

#### blog_log_conversion
block_log_conversion plugin relies on the block_log files.
In the `input` option you have to specify path to a non-empty input original block_log file and in the `output` option you should specify path to a output block_log file that will contain converted blockchain.

#### node_based_conversion
node_based_conversion plugin relies on the remote nodes.
In the `input` option you have to specify an url to the original input mainnet node and in the `output` option you should specify an url to the node with altered chain. If you are converting old transactions, use `use-now-time` option in order to alter the expiration time of the transactions to the current timestamp for nodes to accept them.

This retrieves blocks from the remote and holds them in the block buffer. You can specify the block buffer size with the `block-buffer-size` option, which defaults to 1000.

Note: url should have format: `http://ip-or-host:port`. If you want to use default http port use `80`.

Another note: This conversion tool currently does not support `https` protocol and chunked transfer encoding in API node answers

### Multithreading support
Signing transactions takes relatively large amount of time, so this tool enables multithreading support.
Normally it uses only 1 signing thread. If you want to set it to more, use `jobs` option and specify the number of signing threads.

### Stopping and resuming the conversion
If you want to stop the conversion at specific block, you have to provide the `stop-block` option.
If you want to resume it later, converter will automatically retrieve the last block number if `resume-block` option has not been specified or is equal to 0.
If you are using [blog_log_conversion plugin](#blog_log_conversion) this value will be equal to the last block in the output block log + 1 and if you are using [node_based_conversion plugin](#node_based_conversion) it will be equal to the last head_block_number retrieved from the database_api.get_dynamic_global_properties output node API call + 1.

Note: If you stop the conversion before hardfork 17, it will most propably corrupt the converter state and you will be unable to resume the conversion if you want the block_log to be valid. If you stop it before hardfork 17 and want to resume, you will have to restart the conversion from the beginning.

### Logging
You can log blocks before and after conversion every n blocks using `log-per-block` option or log only specific block using `log-specific`

## Example run
#### block_log_conversion plugin
Convert `block_log` to `new_fancy_block_log` with chain id `1` and private key `5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n` and configure for using same key for second authority as given block signing private key:
```
blockchain_converter --plugin block_log_conversion --input block_log --output new_fancy_block_log --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
#### node_based_conversion plugin
Convert blocks from `api.deathwing.me` and send converted transactions to `127.0.0.1` node with same private keys and chain id from [block_log_conversion plugin example run](#block_log_conversion%20plugin)
```
blockchain_converter --plugin node_based_conversion --input 'http://api.deathwing.me:80' --output 'http://127.0.0.1:80' --chain-id 1 --private-key 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n --use-same-key
```
