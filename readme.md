## Installation

Run installation script. It will install TestTools and help you to select which hived, cli_wallet and such executables should be used by TestTools.
```bash
python3 hive/tests/test_tools/install.py
```

## Features

#### Easy testnet creation
You can run testnet with node configured for blocks production and attached wallet with such simple script:
```python
from test_tools import World

if __name__ == '__main__':
    with World() as world:
        node = world.create_init_node()
        node.run()

        wallet = node.attach_wallet()
```

#### Node and wallet APIs
> :warning: APIs are not fully implemented and contains only few selected calls.

Node and wallet has `api` member which allows for communication with them. Use your IDE's code completion to get hints like below. IDE should help you to write method names, but also parameters.

![Wallet api code completion example](./documentation/wallet_code_completion.png)

#### Node configuration
Node has `config` member which allow for editing _hived_ _config.ini_ file. You can configure node in following way:
```python
node.config.enable_stale_production = True
node.config.required_participation = 0
node.config.witness.append('initminer')
```

> :warning: Type support is not completed yet. Not all config entries types are set correctly. At the moment most of them are treated as strings. So you have to write like:
> ```python
> # Note that all are strings
> node.config.market_history_bucket_size = '[15,60,300,3600,86400]'
> node.config.webserver_thread_pool_size = '32'
> node.config.witness_skip_enforce_bandwidth = '1'
> ```

Provides support for Python types. You can write:
```python
if node.config.enable_stale_production and node.config.required_participation < 20:
    ...
```
because type of `node.config.enable_stale_production` is `bool` and type of `node.config.required_participation` is `int`.

#### Select which executables should library use
You can select them in python script, via command line arguments, environment variables or by executables installation ([read more](documentation/paths_to_executables.md)).

#### Features waiting for description
- Logger
- Key generation with Account
- Network
- MainNet
