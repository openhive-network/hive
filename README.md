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

## Dockerized Consensus Node

To run a Hive node (ca. 24GB of memory is required at the moment):

    docker run \
        -d -p 2001:2001 -p 8090:8090 --name hived \
        hiveio/hive

    docker logs -f hived  # follow along

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
