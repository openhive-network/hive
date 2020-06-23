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
* HBD - Hive's very own stable coin with a one-way peg
* Delegated Proof-of-Stake Consensus (DPoS)
* 10% APR inflation narrowing to 1% APR over 20 years
    * 65% of inflation to authors/curators.
    * 15% of inflation to stakeholders.
    * 10% of inflation to block producers.
    * 10% of inflation to Hive Fund.

# Installation

Getting started with Hive is fairly simple. You can either choose to use docker-images, build with docker manually or build from source directly. All steps have been documented and while many different OS are supported, the easiest one is Ubuntu 18.04.

## Quickstart

Just want to get up and running quickly? We have pre-built Docker images for your convenience. More details are in our [Quickstart Guide](doc/exchangequickstart.md).

## Building

We **strongly** recommend using one of our pre-built Docker images or using Docker to build Hive. Both of these processes are described in the [Quickstart Guide](doc/exchangequickstart.md).

But if you would still like to build from source, we also have [build instructions](doc/building.md) for Linux (Ubuntu LTS) and macOS.

## Dockerized P2P Node

To run a p2p node (ca. 2GB of memory is required at the moment):

    docker run \
        -d -p 2001:2001 -p 8090:8090 --name hived-default \
        openhive-network/hive

    docker logs -f hived-default  # follow along

## Dockerized Full Node

To run a node with *all* the data (e.g. for supporting a content website)
ca. 14GB of memory, and growing, is required:

    docker run \
        --env USE_WAY_TOO_MUCH_RAM=1 --env USE_FULL_WEB_NODE=1 \
        -d -p 2001:2001 -p 8090:8090 --name hived-full \
        openhive-network/hive

    docker logs -f hived-full

## CLI Wallet

We provide a basic cli wallet for interfacing with `hived`. The wallet is self-documented via command line help. The node you connect to via the cli wallet needs to be running the `account_by_key_api`, `condenser_api`, and needs to be configured to accept WebSocket connections via `webserver-ws-endpoint`.

## Testing

See [doc/devs/testing.md](doc/devs/testing.md) for test build targets and info
on how to use lcov to check code test coverage.

# Configuration

## Config File

Run `hived` once to generate a data directory and config file. The default location is `witness_node_data_dir`. Kill `hived`. It won't do anything without seed nodes. If you want to modify the config to your liking, we have two example configs used in the docker images. ( [consensus node](contrib/config-for-docker.ini), [full node](contrib/fullnode.config.ini) ) All options will be present in the default config file and there may be more options needing to be changed from the docker configs (some of the options actually used in images are configured via command line).

## Seed Nodes

A list of some seed nodes to get you started can be found in
[doc/seednodes.txt](doc/seednodes.txt).

This same file is baked into the docker images and can be overridden by
setting `HIVED_SEED_NODES` in the container environment at `docker run`
time to a whitespace delimited list of seed nodes (with port).


## Environment variables

There are quite a few environment variables that can be set to run hived in different ways:

* `USE_WAY_TOO_MUCH_RAM` - if set to true, hived starts a 'full node'
* `USE_FULL_WEB_NODE` - if set to true, a default config file will be used that enables a full set of API's and associated plugins.
* `USE_NGINX_FRONTEND` - if set to true, this will enable an NGINX reverse proxy in front of hived that proxies WebSocket requests to hived. This will also enable a custom healthcheck at the path '/health' that lists how many seconds away from current blockchain time your node is. It will return a '200' if it's less than 60 seconds away from being synced.
* `USE_MULTICORE_READONLY` - if set to true, this will enable hived in multiple reader mode to take advantage of multiple cores (if available). Read requests are handled by the read-only nodes and write requests are forwarded back to the single 'writer' node automatically. NGINX load balances all requests to the reader nodes, 4 per available core. This setting is still considered experimental and may have trouble with some API calls until further development is completed.
* `HOME` - set this to the path where you want hived to store it's data files (block log, shared memory, config file, etc). By default `/var/lib/hived` is used and exists inside the docker container. If you want to use a different mount point (like a ramdisk, or a different drive) then you may want to set this variable to map the volume to your docker container.

## PaaS mode

Hived now supports a PaaS mode (platform as a service) that currently works with Amazon's Elastic Beanstalk service. It can be launched using the following environment variables:

* `USE_PAAS` - if set to true, hived will launch in a format that works with AWS EB. Containers will exit upon failure so that they can be relaunched automatically by ECS. This mode assumes `USE_WAY_TOO_MUCH_RAM` and `USE_FULL_WEB_NODE`, they do not need to be also set.
* `S3_BUCKET` - set this to the name of the S3 bucket where you will store shared memory files for hived in Amazon S3. They will be stored compressed in bz2 format with the file name `blockchain-$VERSION-latest.tar.bz2`, where $VERSION is the release number followed by the git short commit hash stored in each docker image at `/etc/hivedversion`.
* `SYNC_TO_S3` - if set to true, the node will function to only generate shared memory files and upload them to the specified S3 bucket. This makes fast deployments and autoscaling for hived possible.


## System Requirements

[To Be Added]

On Linux use the following Virtual Memory configuration for the initial sync and subsequent replays. It is not needed for normal operation.

```
echo    75 | sudo tee /proc/sys/vm/dirty_background_ratio
echo  1000 | sudo tee /proc/sys/vm/dirty_expire_centisecs
echo    80 | sudo tee /proc/sys/vm/dirty_ratio
echo 30000 | sudo tee /proc/sys/vm/dirty_writeback_centisecs
```

# No Support & No Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
