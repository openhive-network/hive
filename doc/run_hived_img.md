General usage: `run_hived_img.sh <docker_img> [OPTION[=VALUE]]... [<hived_option>]...`

`run_hived_img.sh` can take the following parameters:

```
<img_name>              - name of image to create a container from (required)
--name=<container_name> - specify a specific container name
--docker-option=OPTION  - pass OPTION directly to the underlying docker run
```

Options to create API and P2P endpoints (these also create the appropriate docker port mappings for the container)

```
--webserver-http-endpoint=<endpoint>
--webserver-ws-endpoint=<endpoint>
--p2p-endpoint=<endpoint>
```

Options to specify working directories for the node (the script will pass the appropriate volume mappings to the underlying docker run command based on the following options):


`--data-dir=DIRECTORY_PATH` - specifies the blockchain data directory. See deployment scenarios described below for details on how to prepare this directory<br>
`--shared-file-dir=DIRECTORY_PATH` - specifies the directory where dockerized hived will store its state file (shared_memory.bin). For best performance results, it should be located on a ram-disk (e.g. a subdirectory created under /dev/shm).

### Examining the hived log file

The newly created container (and hived process) will create a log file (hived.log) located in the blockchain data directory (e.g. /home/hived/datadir/hived.log).
By default, the container is always started in attached mode. You can detach it by using `<Ctrl>-p` `<Ctrl>-q` shortcuts as described in docker attach documentation.
If you would like to start the container in detached mode, you can pass the --detach option to the run_hived_img.sh script.

Logs can be also examined using the docker logs feature:

`docker logs -f hived-instance`

### Stopping the hived container

To stop the container, you should use the command `docker stop hived-instance`. A successfully stopped container should leave the message ***exited cleanly*** in the hived.log file. For example:

```
< some log contents >
Shutting down...
Leaving application main loop...
exited cleanly 
```

### Scenarios for using dockerized hived (assuming mainnet configuration)

*Note:* In all scenarios, the dockerized hived process must have write access to the data directory and the shared memory storage directory.

1. Full peer-to-peer (P2P) sync

    In this case, all you need is to specify an empty data directory and the storage directory for the shared memory file. The blockchain directory will be written to by the hived node as it performs a full P2P sync.

    `run_hived_img.sh my-local-image --name=hived-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001`

    The specified data directory cannot contain a *block_log* or a *block_log.artifacts* file in the `blockchain` subdirectory, otherwise the hived node will fail at startup.

2. Full replay from an existing block_log file (and optionally an existing block_log.artifacts file)

    To perform a replay, you need to copy the block_log and block_log.artifacts files into the node's data-dir/blockchain directory. If no block_log.artifacts file is supplied, it will be auto-generated from the block_log file (but this will increase the replay time, so it is usually preferable to include it). To run a replay, you need to pass the `--force-replay` option to the `run_hived_img.sh` script.

    Below is an example command line for running a full replay. It requires a /home/hived/datadir/blockchain directory containing the block_log and block_log.artifacts files. It also requies a /dev/shm/hived/consensus_node directory to exist.

    `run_hived_img.sh my-local-image --name=hived-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --force-replay`

3. Customizing the hived node's configuration

    You can configure the hived node in various ways by specifying a custom config.ini file in the datadir. The config.ini file can specified which plugins to use and the plugin options. If no config.ini file is present in the datadir, a default one will be generated automatically. Important points to remember when providing a custom config.ini file:
    - Don't edit the shared memory file storage directory in the config.ini file. If you want to specify a different one, pass the new one as an option to the run_hived_img.sh script.
    - Simiarly, the port configuration options in the config.ini should not be changed from their default values, because the dockerized hived process will always use the default ports. If you want the dockerized container to use other ports, specify external port mappings using these script options: --webserver-http-endpoint, --webserver-ws-endpoint, and --p2p-endpoint
    - Any snapshot and account history storage directories must be specified relative to the docker internal `/home/hived/datadir` directory which is mapped to the host data directory.
    
    Other hived options can be passed directly on the run_hived_img.sh command line or explicitly specified in the config.ini file.

4. Running an API node and an associated cli_wallet in network daemon mode

    In this scenario we will create a single container running both a hived node and an associated cli_wallet. The cli_wallet will be exposed on an external port, allowing other programs to communicate with it.
    - Start a hived container instance running hived with the set of plugins required by the cli_wallet and a port mapping to expose the port for the cli_wallet (e.g. -p 8093:8093). It is important to specify this optional port mapping at container start up, because it can't be changed later without restarting the container.
    ```
    ./run_hived_img.sh my-local-image --name=api-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/api_node \
      --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 \
      --docker-option="-p 8093:8093" \
      --plugin=account_history_api --plugin=database_api --plugin=condenser_api --plugin=account_by_key_api --plugin=wallet_bridge_api \
      --force-replay
    ```
    - Start a cli_wallet inside the previously-launched container instance.
    
    ```
    ./run_cli_wallet_img.sh api-instance \
      --rpc-http-endpoint=0.0.0.0:8093 # Expose the cli_wallet HTTP server at port 8093. This port value should match to the port specified by the `--docker-option="-p 8093:8093"` passed to the run_hived_img.sh script.
      --rpc-http-allowip=172.17.0.1 # For security reasons, it is best to specify this option which only allows the cli_wallet to accept HTTP connections from the given IPs (option can be repeated to specify multiple IPs). This example value covers the default docker bridge network address mode, allowing the docker host to communicate with the cli_wallet. Note: on some docker systems, the docker network is configured differently and the host has a different IP address. In that case, you can lookup the host ip using the `docker container inspect api-instance` command and find the IP address in the Networks property.
    ```

    By default, the run_cli_wallet_img.sh script will instruct the cli_wallet to load/store its data (e.g. password and  imported keys to operate on) in the `wallet.json` file located in the hived data-dir directory. If you already have an existing wallet.json file, copy it to the data-dir before starting the cli_wallet.

    Then you can operate the cli_wallet using network API calls like these:
    
    ```
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "info", "params": []}'
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "unlock", "params": ["my-secres-pass"]}'
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "create_account", "params":  ["initminer", "alice", "{}", True]}'
    ```
