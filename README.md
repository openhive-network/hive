# Hive - Decentralizing the exchange of ideas and information

![Hive](https://files.peakd.com/file/peakd-hive/netuoso/jMHldwMV-horizontal.png)

Hive is a Graphene based, social blockchain that was created as a fork of Steem and born on the core idea of decentralization. Originally, Hive was announced on the [Steem blockchain](https://peakd.com/communityfork/@hiveio/announcing-the-launch-of-hive-blockchain) prior to the initial token airdrop. Hive did not have any ICO or mining period.

The Hive blockchain removes the elements of centralization and imbalanced control that have plagued the Steem blockchain over the last 4 years. Since it’s launch on March 20, 2020 Hive is growing and evolving day by day. Hive's prime selling points are its decentralization, 3 second transaction speed and ability to handle large volumes. It is ideal real estate for a variety of innovative projects focused on a broad range of fields, from open source development to games.

Hive serves as the operational home for all kinds of projects, companies, and applications. Having a highly active and passionate community, Hive has become a thriving atmosphere for new and experienced developers to quickly bootstrap their applications. On top of this, Hive is extremely rewarding to content creators and curators alike.

Hive aims to be the preferred blockchain for dApp development with Smart Media Tokens at its core. With SMTs, everyone can leverage the power of Hive.

The technical development of the Hive blockchain itself is carried out by the founding decentralized group of over 30 open source developers, many of whom were instrumental in creating Steem back in 2016, and supported by a growing community of additional open source developers and witnesses.

## Documents

* Developer Portal: https://developers.hive.io/

## Advantages

* Hive Fund
* Truly Decentralized Community
* Free Transactions (Resource Credits = Freemium Model)
* Fast Block Confirmations (3 seconds)
* Time Delay Security (Vested Hive & Savings)
* Hierarchical Role Based Permissions (Keys)
* Integrated Token Allocation
* Smart Media Tokens (**soon**)
* Lowest Entry-Barrier for User Adoption in the market
* Dozens of dApps already built on Hive and many more to come

## Technical Details

* Currency symbol HIVE
* HBD - Hive's very own stable coin with a two-way peg
* Delegated Proof-of-Stake Consensus (DPoS)
* 10% APR inflation narrowing to 1% APR over 20 years
    * 65% of inflation to authors/curators.
    * 15% of inflation to stakeholders.
    * 10% of inflation to block producers.
    * 10% of inflation to Hive Fund.

# Installation

Getting started with Hive is fairly simple. You can either choose to use docker-images, build with docker manually or build from source directly. All steps have been documented and while many different OS are supported, the easiest one is Ubuntu 20.04 LTS.

## Quickstart

Just want to get up and running quickly? We have pre-built Docker images for your convenience. More details are in our [Quickstart Guide](doc/exchangequickstart.md).

## Building

We **strongly** recommend using one of our pre-built Docker images or using Docker to build Hive. Both of these processes are described in the [Quickstart Guide](doc/exchangequickstart.md).

But if you would still like to build from source, we also have [build instructions](doc/building.md) for Linux (Ubuntu LTS).

## Dockerized deployment

Building a docker image is described here: [Building under Docker](https://gitlab.syncad.com/hive/hive/-/blob/master/doc/building.md#building-under-docker)

To run a Hive consensus node there are needed resources:
- data directory to hold a blockchain file(s) (ca 400GB is required)
- storage to hold a shared memory file (ca. 24GB of memory is required at the moment to store state data):

There is provided a script wrapping `docker run` statement and emulating direct hived usage: [run_hived_img.sh](https://gitlab.syncad.com/hive/hive/-/blob/master/scripts/run_hived_img.sh)

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
    Container is always started in detached mode.

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

## CLI Wallet

We provide a basic cli wallet for interfacing with `hived`. The wallet is self-documented via command line help. The node you connect to via the cli wallet needs to be running the `account_by_key_api`, `condenser_api`, `wallet_bridge_api` and needs to be configured to accept WebSocket connections via `webserver-ws-endpoint`.

## Testing

See [doc/devs/testing.md](doc/devs/testing.md) for test build targets and info
on how to use lcov to check code test coverage.

# Configuration

## Config File

Run `hived` once to generate a data directory and config file. The default data directory location is `~/.hived`. Kill `hived`. If you want to modify the config to your liking, we have [example config](contrib/config-for-docker.ini) used in the docker image. All options will be present in the default config file and there may be more options needing to be changed from the docker configs (some of the options actually used in images are configured via command line).

## Seed Nodes

A list of some seed nodes to get you started can be found in
[doc/seednodes.txt](doc/seednodes.txt).

This same file is baked into the docker images and can be overridden by
setting `HIVED_SEED_NODES` in the container environment at `docker run`
time to a whitespace delimited list of seed nodes (with port).

## System Requirements

[To Be Added]

# No Support & No Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
