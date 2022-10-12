Exchange Quickstart
-------------------

System Requirements: A dedicated server or virtual machine with a minimum of 24GB of RAM, and at least 600GB of fast **local**  storage (such as SSD or NVMe). Hive is one of the most active blockchains in the world and handles an incredibly large amount of transactions per second, as such, it requires fast storage to run efficiently.

With the right equipment and technical configuration a reindex should take **no longer than 36 hours**.  If recommendations are not followed precisely, the reindex can drag on for days or even weeks with significant slowdowns towards the end.

Physically attached NVMe will ensure an optimal reindex time. NVMe over a NAS or some kind of network storage backed by NVMe will often have much higher latency. As an example, AWS EBS is not performant enough. A good recommended instance in AWS is the i3.xlarge, it comes with a physically attached NVMe drive (it must be formatted and mounted on instance launch).

You can save a lot of time by replaying from a `block_log`. Using the docker method below, we have made it easy to download a `block_log` at launch and replay from it by passing in the `USE_PUBLIC_BLOCKLOG=1` environment variable. To do this, make sure your `blockchain` directory is empty and does not contain a `block_log`. If you are not using docker, you can download a `block_log` from [here](https://gtg.openhive.network/get/blockchain), put it in your Hive data directory, and use the `--replay-blockchain` command line option. Be sure to remove the option if you have to stop/restart hived after already being synced.

We recommend using docker to both build and run Hive for exchanges. Docker is the world's leading containerization platform and using it guarantees that your build and run environment is identical to what our developers use. You can still build from source and you can keep both blockchain data and wallet data outside of the docker container. The instructions below will show you how to do this in just a few easy steps.

### Install docker and git (if not already installed)

You can install docker with the native package manager or with the script from get.docker.com:
```
curl -fsSL get.docker.com -o get-docker.sh
sh get-docker.sh
```

### Using single-step script

Hive repository contains a script which allows to setup an exchange node in one step, also in several variants, altough all scenarios are based on dockerized setup. The `build_and_setup_exchange_instance.sh` script allows to:

A) optionally clone sources. This is default behavior. You can specify `--branch=<branch-name>` option, to specify different branch than master
B) optionally use already checked out sources to build a docker image. This can be achieved by using `--use-source-dir=<hive_hf26_source>`
C) or just reuse already built docker image to start a container holding hived node - that happens every time when specified image tag already exists on your machine - image build process is skipped

Script also allows automatic download of block_log file(s) before starting hived replay process. To do it, please specify: `--download-block-log` option.

Please call `./scripts/build_and_setup_exchange_instance.sh --help` to list all available options. There is supported passing options using an option file (see `--option-file=PATH`). 
This script by default uses a `scripts/exchange_instance.conf` file, containing default Hived options needed to setup an API instance specific to exchange needs (including activation of AccountHistory plugin and definition of account filters)

Deployment scenarios:

1) Starting from scratch:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-p2p --data-dir="$(pwd)/datadir/"`

Above call will perform a Hive repository clone, then verify existance of given image and build it if needed. Finally, a docker container called `exchange-instance-p2p` will be started, where hived will be gathering blocks from scratch using P2P layer.

2) Downloading a block_log and performing a replay:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-replay --data-dir="$(pwd)/datadir/" --download-block-log --replay-blockchain`

Above call will perform a Hive repository clone, then verify existance of given image and build it if needed. Finally, a docker container called `exchange-instance-replay` will be started, where hived will be evaluating blocks using a block_log downloaded at previous step.

3) Using an existing block_log and performing a replay:

`../hive-26/scripts/build_and_setup_exchange_instance.sh local-exchange --name=exchange-instance-replay --use-source-dir=../hive-26 --data-dir="$(pwd)/datadir/" --replay-blockchain`

In this scenario, inside directory `datadir` must exists `blockchain` subdirectory, containing a block_log file(s).
Above call uses already cloned sources, then verify existance of given image and build it if needed. Finally, a docker container called `exchange-instance-replay` will be started, where hived will be evaluating blocks using a block_log downloaded at previous step.

Both scenarios using --replay-blockchain, allows to continue previously stopped replay process - starting this script again should resume replay process at last processed block (if the same data directory holding shared memory file will be specified).

Script accepts all options provided by hived. If you want to specify different p2p or http port, please use `--p2p-endpoint=` or `--webserver-http-endpoint=` or `--webserver-ws-endpoint=` options, since script will transpose them into docker port mappings accordingly.

To start a cli_wallet in the dockerized environment, simplest solution is to use [run_cli_wallet_img.sh](://gitlab.syncad.com/hive/hive/-/blob/master/scripts/run_cli_wallet_img.sh). It assumes that wallet.json file should be located in the data-dir mapped to previously started hived container. So using paths specified in the exampole, you should **copy** it into `"$(pwd)/datadir/` directory and next start script.

cli_wallet can be started in the interactive mode (default) or network daemon (handling JSON calls). To start a network daemon let's use `--rpc-http-endpoint=0.0.0.0:8093` and `--rpc-http-allowip=172.17.0.1` (default docker network IP range - please verify it before use).

### Performing each setup steps separately.

This kind of setup mostly is covered by general build procedures - described in [building.md](https://gitlab.syncad.com/hive/hive/-/blob/master/doc/building.md#building-under-docker).

If you'd like to use our already pre-built official binary images, it's as simple as downloading it from the Dockerhub registry with only one command:

```
docker pull hiveio/hive
```

Running a docker image, in the API node case (specific to exhange deployment) like also using dockerized version of cli_wallet is described [here](https://gitlab.syncad.com/hive/hive/-/blob/master/README.md#scenarios-of-using-dockerized-hived-assumed-mainnet-configuration)

### Running a binary build without a Docker container

If you build with Docker but do not want to run hived from within a docker container, you can stop here with this step and instead extract the binary from the container with the commands below. If you are going to run hived with docker (recommended method), skip this step altogether. We're simply providing an option for everyone's use-case. Our binaries are built mostly static, only dynamically linking to linux kernel libraries. We have tested and confirmed binaries built in Docker work on Ubuntu and Fedora and will likely work on many other Linux distrubutions. Building the image yourself or pulling one of our pre-built images both work.

To extract the binary you can use [export-binaries.sh](https://gitlab.syncad.com/hive/hive/-/blob/master/scripts/export-binaries.sh) or start a container and then copy the file from it:

```
docker run -d --name hived-exchange hiveio/hive
docker cp hived-exchange:/home/hived/bin/hived /local/path/to/hived
docker cp hived-exchange:/home/hived/bin/cli_wallet /local/path/to/cli_wallet
docker stop hived-exchange
```

### Configuration file

For your convenience, we have provided a provided an [example\_config](example\_config.ini) that we expect should be sufficient to run your exchange node. Be sure to rename it to simply `config.ini`. Be sure to set the account name of your wallet account that you would like to track account history for in the config file. It is defined as `account-history-rocksdb-track-account-range = ["accountname","accountname"]`.
If you want to use custom configuration while using docker, you can place this outside of your container and copy it into a data-dir mapped to your container.

### Upgrading for major releases that require a full reindex

For upgrades that require a full replay, we highly recommend *performing the upgrade on a separate server* in order to minimize downtime of your wallet. When the replay is complete, switch to the server running the newer version of Hive. If for some reason it is absolutely not possible to perform the upgrade on a separate server, you would use the following instructions instead:

Stop the docker container, remove the existing container, clear out your blockchain data directory completely, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with.

```
docker stop hived-exchange
docker rm hived-exchange
rm -rf datadir/blockchain/account-history-rocksdb-storage datadir/blockchain/block_log.index datadir/blockchain/shared_memory.bin
touch datadir/blockchain/force_replay 
docker pull hiveio/hive
run_hived_img.sh hiveio/hive --name=hived-exchange --docker-option="-p 8093:8093" --data-dir=$(pwd)/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 --force-replay
```

### Upgrading for releases that do not require a reindex

For upgrades that do not require a full replay, you would use the following instructions: stop the docker container, remove the existing container, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with:

```
docker stop hived-exchange
docker rm hived-exchange
docker pull hiveio/hive
run_hived_img.sh hiveio/hive --name=hived-exchange --docker-option="-p 8093:8093" --data-dir=$(pwd)/datadir --shared-file-dir=/dev/shm/hived/consensus_node --webserver-http-endpoint=0.0.0.0:8091 --webserver-ws-endpoint=0.0.0.0:8090 --p2p-endpoint=0.0.0.0:2001 
```
