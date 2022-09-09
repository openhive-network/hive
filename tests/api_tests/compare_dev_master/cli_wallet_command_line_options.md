### Changes in cli_wallet **command-line** options:

*New:*
- `-v`, `[ --version ]` - print git revision sha of this cli_wallet build,
- `-o`, `[ --offline ]`  - run the wallet in offline mode,
- `-s`, `[ --server-rpc-endpoint ] arg (=ws://127.0.0.1:8090)` - server websocket RPC endpoint,
- `--output-formatter arg` - allows to present a result in different ways. Possible values(none/text/json),
- `--transaction-serialization arg (=legacy)` -allows to generate JSON using legacy/HF26 format. Possible values(legacy/hf26). By default is `legacy`,
- `--store-transaction arg` - requires a name of file. Allows to save a serialized transaction into the file in a current directory. By default a name of file is empty.

*Syntax changes:*
- `-s [ --server-rpc-endpoint ] arg (=ws://127.0.0.1:8090)` -> `-s [ --server-rpc-endpoint ] [=arg(=ws://127.0.0.1:8090)]` - Server websocket RPC endpoint.
