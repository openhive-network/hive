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

### Clone the hive repo

Pull in the hive repo from the official source on github and then change into the directory that's created for it.
```
git clone https://github.com/openhive-network/hive
cd hive
```

### Build the image from source with docker

Docker isn't just for downloading already built images, it can be used to build from source the same way you would otherwise build. By doing this you ensure that your build environment is identical to what we use to develop the software. Use the below command to start the build:

```
docker build -t=hiveio/hive --target=consensus_node .
```

Don't forget the `.` at the end of the line which indicates the build target is in the current directory.

### Using our official Docker images without building from source

If you'd like to use our already pre-built official binary images, it's as simple as downloading it from the Dockerhub registry with only one command:

```
docker pull hiveio/hive
```

### Running a binary build without a Docker container

If you build with Docker but do not want to run hived from within a docker container, you can stop here with this step and instead extract the binary from the container with the commands below. If you are going to run hived with docker (recommended method), skip this step altogether. We're simply providing an option for everyone's use-case. Our binaries are built mostly static, only dynamically linking to linux kernel libraries. We have tested and confirmed binaries built in Docker work on Ubuntu and Fedora and will likely work on many other Linux distrubutions. Building the image yourself or pulling one of our pre-built images both work.

To extract the binary you need to start a container and then copy the file from it.

```
docker run -d --name hived-exchange hiveio/hive
docker cp hived-exchange:/usr/local/hive/consensus/bin/hived /local/path/to/hived
docker cp hived-exchange:/usr/local/hive/consensus/bin/cli_wallet /local/path/to/cli_wallet
docker stop hived-exchange
```

### Configuration file

For your convenience, we have provided a provided an [example\_config](example\_config.ini) that we expect should be sufficient to run your exchange node. Be sure to rename it to simply `config.ini`. Be sure to set the account name of your wallet account that you would like to track account history for in the config file. It is defined as `account-history-rocksdb-track-account-range = ["accountname","accountname"]`.
If you want to use custom configuration while using docker, you can place this outside of your container and map to it by adding this argument to your docker run command: `-v /path/to/config.ini:/usr/local/hive/consensus/datadir/config.ini`.

### Create directories to store blockchain and wallet data outside of Docker

For re-usability, you can create directories to store blockchain and wallet data and easily link them inside your docker container.

```
mkdir datadir
mkdir hivewallet
```

### Run the container

The below command will start a daemonized instance opening ports for p2p and RPC while linking the directories we created for blockchain and wallet data inside the container. Fill in `TRACK_ACCOUNT` with the name of your exchange account that you want to follow. The `-v` flags are how you map directories outside of the container to the inside, you list the path to the directories you created earlier before the `:` for each `-v` flag. The restart policy ensures that the container will automatically restart even if your system is restarted.

```
docker run -d --name hived-exchange --env TRACK_ACCOUNT=nameofaccount --env USE_PUBLIC_BLOCKLOG=1 -p 2001:2001 -p 8090:8090 -v /path/to/hivewallet:/var/hivewallet -v /path/to/datadir:/usr/local/hive/consensus/datadir -v /home/exchange/datadir/config.ini:/usr/local/hive/consensus/datadir/config.ini --restart always hiveio/hive
```

You can see that the container is running with the `docker ps` command.

To follow along with the logs, use `docker logs -f`.

Initial syncing will take between 12 and 72 hours depending on your equipment, faster storage devices will take less time and be more efficient. Subsequent restarts will not take as long.

### Running the cli_wallet

The command below will run the cli_wallet from inside the running container while mapping the `wallet.json` to the directory you created for it on the host.

```
docker exec -it hived-exchange /usr/local/hive/consensus/bin/cli_wallet -w /var/hivewallet/wallet.json
```

### Upgrading for major releases that require a full reindex

For upgrades that require a full replay, we highly recommend *performing the upgrade on a separate server* in order to minimize downtime of your wallet. When the replay is complete, switch to the server running the newer version of Hive. If for some reason it is absolutely not possible to perform the upgrade on a separate server, you would use the following instructions instead:

Stop the docker container, remove the existing container, clear out your blockchain data directory completely, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with.

```
docker stop hived-exchange
docker rm hived-exchange
rm -rf datadir/blockchain/account-history-rocksdb-storage datadir/blockchain/block_log.index datadir/blockchain/shared_memory.bin
touch datadir/blockchain/force_replay 
docker pull hiveio/hive
docker run -d --name hived-exchange --env TRACK_ACCOUNT=nameofaccount --env USE_PUBLIC_BLOCKLOG=1 -p 2001:2001 -p 8090:8090 -v /path/to/hivewallet:/var/hivewallet -v /path/to/datadir:/usr/local/hive/consensus/datadir -v /home/exchange/datadir/config.ini:/usr/local/hive/consensus/datadir/config.ini --restart always hiveio/hive
```

### Upgrading for releases that do not require a reindex

For upgrades that do not require a full replay, you would use the following instructions: stop the docker container, remove the existing container, pull in the latest docker image (or build the image from scratch), and then start a new container using the same command that you previously launched with:

```
docker stop hived-exchange
docker rm hived-exchange
docker pull hiveio/hive
docker run -d --name hived-exchange --env TRACK_ACCOUNT=nameofaccount --env USE_PUBLIC_BLOCKLOG=1 -p 2001:2001 -p 8090:8090 -v /path/to/hivewallet:/var/hivewallet -v /path/to/datadir:/usr/local/hive/consensus/datadir -v /home/exchange/datadir/config.ini:/usr/local/hive/consensus/datadir/config.ini --restart always hiveio/hive
```
