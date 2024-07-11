Exchange Quickstart
-------------------

System Requirements: A dedicated server or virtual machine with a minimum of 24GB of RAM, and at least 600GB of fast **local**  storage (such as SSD or NVMe). Hive is one of the most active blockchains in the world and handles an incredibly large amount of transactions per second, as such, it requires fast storage to run efficiently.

With the right equipment and technical configuration a replay should take **no longer than 36 hours**.  If recommendations are not followed precisely, the replay can drag on for days or even weeks with significant slowdowns towards the end.

Physically attached NVMe will ensure an optimal replay time. NVMe over a NAS or some kind of network storage backed by NVMe will often have much higher latency. As an example, AWS EBS is not performant enough. A good recommended instance in AWS is the `i3.xlarge`, it comes with a physically attached NVMe drive (it must be formatted and mounted on instance launch).

You can save a lot of time by replaying from an existing `block_log`. If you don't have one already, you can download a recent `block_log` from [here](https://gtg.openhive.network/get/blockchain)
Put it in the `blockchain` directory within data dir, and use the `--replay-blockchain` command line option.

We recommend using docker to both build and run Hive for exchanges. Docker is the world's leading containerization platform and using it guarantees that your build and run environment is identical to what our developers use. You can still build from source and you can keep both the blockchain data and wallet data outside of the docker container. The instructions below will show you how to do this in just a few easy steps.

### Install docker and git (if not already installed)

You can install docker with the native package manager or with the script from get.docker.com:
```
curl -fsSL get.docker.com -o get-docker.sh
sh get-docker.sh
```

### Using single-step script

The hive repository contains a script which allows setup of an exchange node in one step. There are several variants, all of which use a dockerized setup. The `build_and_setup_exchange_instance.sh` script can be run in one of three ways:

A) Default behavior is to clone and build source from the master branch. You can specify the `--branch=<branch-name>` option to specify a branch other than master.

B) Optionally use already checked out sources to build a docker image using `--use-source-dir=<hive_hf26_source>`.

C) Or reuse an already built docker image to start the container holding the hived node. This happens when the specified image tag already exists on your machine (in this case the image build process is skipped).

The script can also automatically download block_log file(s) before starting the hived replay process using the `--download-block-log` option.

Run `./scripts/build_and_setup_exchange_instance.sh --help` to list all the available options. 

An options file can be used to pass command-line options (see `--option-file=PATH`). 

This script by default uses a `scripts/exchange_instance.conf` file, containing default Hived options needed to setup an API instance specific to exchange needs (including activation of the AccountHistory plugin and definition of exchange account filters).

Deployment scenarios:

1) Starting from scratch:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-p2p --data-dir="$(pwd)/datadir/"`

This command will clone the hive repository, verify existence of the given image (i.e. local-exchange), and builds it if not. Finally, a docker container called `exchange-instance-p2p` will be started and hived begin fetching blocks from scratch using the P2P layer.

2) Downloading a block_log file and performing a replay:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-replay --data-dir="$(pwd)/datadir/" --download-block-log --replay-blockchain`

This command will clone the hive repository, verify existence of the given image, and builds it if not. Finally, a docker container called `exchange-instance-replay` will be started and hived will begin evaluating blocks using the downloaded block_log file.

3) Using an already existing local block_log file and performing a replay:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-replay --use-source-dir=../hive-26 --data-dir="$(pwd)/datadir/" --replay-blockchain`

In this scenario, inside the directory `datadir` must have a `blockchain` subdirectory containing a block_log file.
This command uses already cloned source files, verifies existence of the given image, and builds it if not. Finally, a docker container called `exchange-instance-replay` will be started and hived will begin evaluating blocks using the existing local block_log file.

Both scenarios using --replay-blockchain allow continuation of a previously interrupted replay process. Starting this script again should resume the replay process at the last processed block (assuming the same data directory holding the shared memory file has been specified).

This script accepts all options accepted by hived. If you want to specify different p2p or http ports, please use `--p2p-endpoint=` or `--webserver-http-endpoint=` or `--webserver-ws-endpoint=` options, since the script will transpose them into docker port mappings accordingly.

To start a cli_wallet in the dockerized environment, the simplest solution is to use [run_cli_wallet_img.sh](/scripts/run_cli_wallet_img.sh). It assumes that the wallet.json file should be located in the data-dir mapped to the previously started hived container. Using paths specified in this example, you should **copy** wallet.json into the `"$(pwd)/datadir/` directory and then start the script.

The cli_wallet can be started in interactive mode (default) or in network daemon mode (handling JSON calls). To start a network daemon, use `--rpc-http-endpoint=0.0.0.0:8093` and `--rpc-http-allowip=172.17.0.1` (default docker network IP range - please verify it before use).

### Performing each setup step separately

This kind of setup mostly is covered by general build procedures described in [building.md](/doc/building.md#building-under-docker).

If you'd like to use our already pre-built official binary image, it's as simple as downloading it from the Dockerhub registry with only one command:

```
docker pull hiveio/hive
```

Running a docker image in the API node case (specific to exhange deployment) and using a dockerized version of cli_wallet are both described [here](/README.md#scenarios-of-using-dockerized-hived-assumed-mainnet-configuration)

### Running a binary build without a docker container

If you build with docker but do not want to run hived from within a docker container, you can stop here with this step and instead extract the binary from the container with the commands below. If you are going to run hived with docker (recommended method), skip this step altogether. We're simply providing an option for everyone's use-case. Our binaries are built mostly static, only dynamically linking to linux kernel libraries. We have tested and confirmed that binaries built in docker work on Ubuntu and Fedora and will likely work on many other Linux distributions. Building the image yourself or pulling one of our pre-built images both work.

To extract the binary, you can use [export-data-from-docker-image.sh](/scripts/export-data-from-docker-image.sh) or start a container and then copy the file from it:

```
docker run -d --name hived-exchange hiveio/hive
docker cp hived-exchange:/home/hived/bin/hived /local/path/to/hived
docker cp hived-exchange:/home/hived/bin/cli_wallet /local/path/to/cli_wallet
docker stop hived-exchange
```

### Hived configuration file (config.ini)

For your convenience, we have provided an [example\_config](example\_config.ini) that we expect should be sufficient to run your exchange node. Be sure to rename it to simply `config.ini`. Be sure to set the account name of your wallet account that you would like to track the account history for in the config file. It is defined as `account-history-rocksdb-track-account-range = ["accountname","accountname"]`.
If you want to use a custom config file while using docker, you can place it outside of your container and copy it into the data-dir mapped to your container.

### Upgrading for major releases that require a full replay

For upgrades that require a full replay, we highly recommend *performing the upgrade on a separate server* in order to minimize downtime of your wallet. When the replay is complete, switch to the server running the newer version of Hive. If for some reason it is absolutely not possible to perform the upgrade on a separate server, you would use the following instructions instead:

Stop the docker container, remove the existing container, clear out your blockchain data directory completely, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with.

```
docker stop hived-exchange
docker rm hived-exchange
# block_log.index is an obsolete legacy file that may have been removed earlier
rm -rf datadir/blockchain/account-history-rocksdb-storage datadir/blockchain/block_log.index datadir/blockchain/block_log.artifacts datadir/blockchain/shared_memory.bin
touch datadir/blockchain/force_replay 
docker pull hiveio/hive
run_hived_img.sh hiveio/hive --name=hived-exchange --docker-option="-p 8093:8093" --data-dir=$(pwd)/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --force-replay
```

### Upgrading for releases that do not require a replay

For upgrades that do not require a full replay, you would use the following instructions: stop the docker container, remove the existing container, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with:

```
docker stop hived-exchange
docker rm hived-exchange
docker pull hiveio/hive
run_hived_img.sh hiveio/hive --name=hived-exchange --docker-option="-p 8093:8093" --data-dir=$(pwd)/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 
```
