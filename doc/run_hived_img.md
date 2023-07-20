General usage: `run_hived_img.sh <docker_img> [OPTION[=VALUE]]... [<hived_option>]...`

`run_hived_img.sh` can take following parameters:

<img_name> - name of image to create a container for,
--name=<container_name> - allows to specify explicit container name
--docker-option=OPTION  - allows to pass give OPTION directly to docker run spawn

Options specific to node endpoints (they are also creating appropriete docker port mappings at container spawn)

--webserver-http-endpoint=<endpoint>
--webserver-ws-endpoint=<endpoint>
--p2p-endpoint=<endpoint>

Options to specify node work directories (script will pass appropriete volume mappings to underlying docker run basing on following options):

--data-dir=DIRECTORY_PATH - allows to specify blockchain data directory. See deployment scenarios described below, for details how to prepare this directory
--shared-file-dir=DIRECTORY_PATH - allows to specify a directory where dockerized hived will store its state file (shared_memory.bin). For best performance results, it should be located at ram-disk, inside subdirectory created under /dev/shm resource.

Started container (and hived process) will create a log file: hived.log located in specified data directory (so in examples below: /home/hived/datadir/hived.log).
By default, container is always started in atached mode. You can detach it by using Ctrl-p Ctrl-q shortcuts as described in docker attach documentation
If you would like to start container in detached mode, you can pass --detach option to the run_hived_img.sh script.

Logs can be also examined using docker logs feature:

docker logs -f hived-instance  # follow along

To stop container, you should use a docker stop hived-instance command. Successfully stopped container should leave in the hived.log `exited cleanly` message:

```
< some log contents >
Shutting down...
Leaving application main loop...
exited cleanly 
```

### Scenarios of using dockerized hived (assumed mainnet configuration)

1. Full P2P sync.
    In this case, all what you need is to specify empty data directory and the storage for shared memory file. Blockchain directory will be filled by hived node performing a full P2P sync. Please be sure, that dockerized hived process has write access to both directories

    `run_hived_img.sh my-local-image --name=hived-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001`

    Pointed data directory **must** contain an empty `blockchain` subdirectory otherwise hived node can fail at start.

2. Hived node full replay, basing on local copy of blockchain file(s).
    To perform a replay, you need to copy blockchain file(s) into directory expected by hived node. Dockerized version assumes a data-dir/blockchain location where block_log and block_log.artifacts files should be stored. Dockerized hived process must write access to such directory and files. To run replay, you need to pass `--force-replay` option to the `run_hived_img.sh` script, which next will be passed to the underlying hived process.

    Before starting following command line, there was created and prepared a /home/hived/datadir (containing a blockchain subdirectory with block_log and block_log.artifacts files), like also created /dev/shm/hived/consensus_node directory

    `run_hived_img.sh my-local-image --name=hived-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --force-replay`

3. Hived node configuration customization
    Data directory specified to `run_hived_img.sh` spawn can of course contain a custom config.ini file, where as usually can be specified dedicated set of plugins and their specific options. Otherwise, hived node (also as usually) will generate default one in this directory. Anyway dockerized deployment has some gotchas:
    - data directory, shared memory file directory options are always passed to underlying hived command line, as internal directory located inside docker container
    - port configuration always should be left as is, since dockerized hived process shall always use default ones, which can be externally mapped by specifying --webserver-http-endpoint, --webserver-ws-endpoint, --p2p-endpoint options to the script
    - snapshot, account history storage directories rather shall be specified relatievely to docker internal `/home/hived/datadir` which is mapped to hosted data directory
    
    Other options can be as usually passed directly to the (run_hived_img.sh) command line (as hived-options) or explicitly specified in config.ini file.

4. Running API node and associated cli_wallet in network daemon mode
    To run an API node and next start a cli_wallet cooperating to it, use following steps:
    - start a Hive container instance running hived enabled required set of plugins, like also having additionally mapped the port cli_wallet will be operating on. It is important to specify this optional port mapping at container start, since it can't be changed later, without restarting the container.
    ```
    ./run_hived_img.sh my-local-image --name=api-instance --data-dir=/home/hived/datadir --shared-file-dir=/dev/shm/hived/api_node \
      --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 \
      --docker-option="-p 8093:8093" \
      --plugin=account_history_api --plugin=database_api --plugin=condenser_api --plugin=account_by_key_api --plugin=wallet_bridge_api \
      --force-replay
    ```
    - run a cli_wallet tool, which should execute **inside** docker container started in previous step
    
    ```
    ./run_cli_wallet_img.sh api-instance \
      --rpc-http-endpoint=0.0.0.0:8093 # that means that we want to expose from docker container the cli_wallet HTTP server at port 8093. This port value should match to the port specified at `--docker-option="-p 8093:8093"` passed to run_hived_img.sh
      --rpc-http-allowip=172.17.0.1 # this is very important option, which allows to accept HTTP connections (incoming to cli_wallet) only from given IPs. This example value covers default docker bridge network address mode. Best to check given ip using `docker container inspect api-instance` and verify IP specific to its Networks property
    ```

    By default, the run_cli_wallet_img.sh script will instruct a cli_wallet to load/store its data (like password, imported keys to operate on) in the `wallet.json` file located in the directory specified as data-dir mapping during run_hived_img.sh spawn. You can also copy an existing wallet.json file to the data-dir. Then started cli_wallet will load such preconfigured values from it.

    Then you can operate with cli_wallet using network API calls, like this:
    
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "info", "params": []}'
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "unlock", "params": ["my-secres-pass"]}'
    curl --request POST --url http://<your-container-ip>:8093/ --header 'Content-Type: application/json' --data '{"jsonrpc": "2.0", "id": 0, "method": "create_account", "params":  ["initminer", "alice", "{}", True]}'
