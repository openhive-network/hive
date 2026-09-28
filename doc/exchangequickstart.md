Exchange Quickstart
-------------------

System Requirements: A dedicated server or virtual machine with a minimum of 16GB of RAM, and at least 1TB of fast **local** storage (such as SSD or NVMe). Hive is one of the most active blockchains in the world and handles an incredibly large amount of transactions per second, as such, it requires fast storage to run efficiently.

With the right equipment and technical configuration a replay should take **well under a day**.  If recommendations are not followed precisely, the replay can drag on for days or even weeks with significant slowdowns towards the end.

Physically attached NVMe will ensure an optimal replay time. NVMe over a NAS or some kind of network storage backed by NVMe will often have much higher latency. As an example, AWS EBS is not performant enough. A good recommended instance in AWS is the `i3.xlarge`, it comes with a physically attached NVMe drive (it must be formatted and mounted on instance launch).

### Recommended: Docker Compose

The [docker/exchange](/docker/exchange) directory contains a Docker Compose deployment of an exchange node: `hived` with account history for your own accounts, and an optional `cli_wallet` daemon. Everything is declared in `compose.yml`, `.env` and `config.ini`; no scripts run on the host.

```
git clone --depth=1 https://gitlab.syncad.com/hive/hive.git
cd hive/docker/exchange
cp .env.example .env            # set HIVED_VERSION
$EDITOR config.ini              # set your account name(s)
docker compose up -d
docker compose logs -f hived
```

On the first start the node downloads the published exchange state snapshot (~6GB), loads it, and syncs the rest from the P2P network, reaching the head in well under an hour; set `BLOCK_LOG_URL` in `.env` instead to download and replay the full block log (~550GB) for a node that keeps every block. Its health check reports `healthy` once it has caught up. Data placement, the wallet, upgrades and troubleshooting are covered in [docker/exchange/README.md](/docker/exchange/README.md).

Pre-built images are published on Docker Hub as `hiveio/hive:<version>`. Always pin a version tag; `latest` is deliberately not published because a new release may require a replay.

### Legacy: script-based setup

Earlier releases were started with `build_and_setup_exchange_instance.sh`, `run_hived_img.sh` and `run_cli_wallet_img.sh` from the `scripts` directory. They still work but are superseded by the compose deployment above, which reuses an existing data directory; see the migration section of its README. The scripts are documented in [run_hived_img.md](/doc/run_hived_img.md).

### Running a binary build without a docker container

If you do not want to run hived from within a docker container, you can extract the binary from the image. Our binaries are built mostly static, only dynamically linking to linux kernel libraries. We have tested and confirmed that binaries built in docker work on Ubuntu and Fedora and will likely work on many other Linux distributions.

```
docker create --name hived-extract hiveio/hive:<version>
docker cp hived-extract:/home/hived/bin/hived /local/path/to/hived
docker cp hived-extract:/home/hived/bin/cli_wallet /local/path/to/cli_wallet
docker rm hived-extract
```

### Hived configuration file (config.ini)

[docker/exchange/config.ini](/docker/exchange/config.ini) is the configuration used by the compose deployment and a good starting point for any exchange node. Set the account name of your wallet account that you would like to track the account history for; it is defined as `account-history-rocksdb-track-account-range = ["accountname","accountname"]`, one line per account. Without any such line hived keeps the history of every account on the chain, which needs far more disk and time.

### Upgrading

Upgrades are described in [docker/exchange/README.md](/docker/exchange/README.md#upgrading). For upgrades that require a full replay, we highly recommend *performing the upgrade on a separate server* in order to minimize downtime of your wallet. When the replay is complete, switch to the server running the newer version of Hive.
