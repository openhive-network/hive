# Running an exchange node with Docker Compose

This directory runs a Hive node the way an exchange needs one: a single `hived`
holding consensus state plus account history for your own accounts, and an
optional `cli_wallet` daemon for signing. The whole deployment is three files:

| File           | Purpose                                                        |
|----------------|----------------------------------------------------------------|
| `compose.yml`  | services, volumes, ports, health check                         |
| `.env`         | image version, ports, block log source (copy of `.env.example`) |
| `config.ini`   | hived options, above all the accounts whose history to keep    |

Nothing runs on the host except Docker: `docker compose up -d` is the entire
procedure, and a fresh volume bootstraps itself.

## Requirements

- Docker Engine with Compose v2.24 or newer.
- 16 GB of RAM (32 GB recommended).
- Fast **local** storage such as NVMe. A node with a block log needs about 1 TB:
  the block log is ~550 GB and growing, the state file ~5 GB in use (sparse, 8 GB
  nominal), and account history depends on how many accounts you track. A
  snapshot-only node needs a few tens of GB. Network-attached storage (for example
  AWS EBS) is too slow for a replay; use an instance with physically attached NVMe.
- Outbound internet access for the P2P network and the block log download.
  Accepting inbound connections on TCP 2001 helps the network but is optional.

## First start

1. Copy `.env.example` to `.env` and set `HIVED_VERSION` to the release you want.
   Always pin a version; `latest` is deliberately not published because a new
   release may require a replay.
2. Edit `config.ini`: replace `your-exchange` with your account name. Add one
   `account-history-rocksdb-track-account-range` line per account. This must be
   right before the first start, because changing it later means replaying.
3. Start the node and follow its log:

   ```
   docker compose up -d
   docker compose logs -f hived
   ```

The first start bootstraps the node from whichever source `.env` names, and later
starts leave the data alone, so the command never changes:

- **Block log (default, recommended).** With `BLOCK_LOG_URL` set, the container
  downloads the block log (~550 GB, resumable, safe to interrupt) into its data
  volume and replays it. A replay takes one to two days on NVMe and ends with
  `Done reindexing`; after that hived fetches the remaining blocks from the P2P
  network. The node then holds every block and can be replayed again later.
- **Snapshot.** With `SNAPSHOT_URL` set instead, the container downloads a state
  snapshot (~6 GB), loads it in a couple of minutes, and syncs forward from the
  snapshot's date at roughly a thousand blocks per second. The published exchange
  snapshot already contains account history for the major exchange accounts (see
  the `example-exchange-config.ini` published next to it); it is only useful if
  yours is among them, because history before the snapshot cannot be recovered
  without a block log. A snapshot-only node keeps no block log: it serves state,
  account history and the head block, but `get_block` fails for older blocks with
  "has been pruned", and it cannot replay. Use a snapshot made by the same hived
  version you run.
- **Snapshot and block log.** With both set, the container downloads both, loads
  the snapshot, and hived replays the block log forward from the snapshot's block
  on its own. Same download as the block log alone, but the replay shrinks from
  the whole chain to the months since the snapshot, and the node keeps every block.
- **Neither.** With both empty the node syncs from genesis over P2P, which takes
  days and is the slowest option. Add `block-log-split = 0` to `config.ini` to do
  that without storing a block log.

`docker compose ps` shows the node as `healthy` once its head block is at most
`HIVED_HEALTHCHECK_MAX_BLOCK_AGE` seconds old. Until then it reports `starting`
or `unhealthy`, which is expected while it catches up.

## Using the node

The API listens on the host at `http://127.0.0.1:8091` (JSON-RPC over HTTP) and
`ws://127.0.0.1:8090` (WebSocket). Change `API_BIND_ADDRESS` in `.env` to expose
it on another interface; the API has no rate limiting, so keep it off the
public internet.

```
curl -s --data '{"jsonrpc":"2.0","method":"database_api.get_dynamic_global_properties","id":1}' http://127.0.0.1:8091/
curl -s --data '{"jsonrpc":"2.0","method":"account_history_api.get_account_history","params":{"account":"your-exchange","start":-1,"limit":10},"id":1}' http://127.0.0.1:8091/
```

Only trust results once the node is healthy: a node that is still syncing
answers, but with old data.

## Wallet

Exchanges that build and sign transactions in their own backend with a Hive
library do not need `cli_wallet` at all. Otherwise there are two ways to use it.

**Interactive**, inside the running node's container. The wallet file is kept in
the node's data volume:

```
docker compose exec -it hived /home/hived/bin/cli_wallet --wallet-file=/home/hived/datadir/wallet.json
```

**Daemon mode**, as a JSON-RPC signing service. Set `COMPOSE_PROFILES=wallet` in
`.env` and run `docker compose up -d` again. The daemon listens on the host at
`CLI_WALLET_BIND_ADDRESS:CLI_WALLET_PORT` (default `127.0.0.1:8093`) and accepts
clients from `CLI_WALLET_ALLOW_IP`. Its `wallet.json` lives in the `wallet`
volume, which no other service can see:

```
curl -s --data '{"jsonrpc":"2.0","method":"is_new","params":[],"id":1}' http://127.0.0.1:8093/
docker compose cp wallet.json cli_wallet:/home/hived/datadir/wallet.json && docker compose restart cli_wallet
```

## Where the data lives

`hived-data` holds the block log, account history, logs and the interactive
wallet's `wallet.json`; `hived-shm` holds the state file. Both are named volumes
managed by Docker, so nothing has to be created or chowned on the host.

To keep a volume on a particular disk, point it at an existing directory in
`compose.yml` (the commented example under `volumes:`). hived runs as UID 1000,
so the directory must be writable by that user: `chown 1000 <dir>`, or add
`HIVED_UID: <owner uid>` to the `hived` service's `environment:` to run hived as
the directory's owner instead.

The state file can live in RAM for a faster replay: bind `hived-shm` to a
directory on a host tmpfs such as `/dev/shm/hived` (allow 10 GB). It survives
container restarts but not a host reboot, after which hived replays from the
block log. Do not use a compose `tmpfs:` mount for it; that is discarded every
time the container is recreated.

## Upgrading

1. Set `HIVED_VERSION` in `.env` to the new release.
2. `docker compose pull && docker compose up -d`. The node is stopped cleanly
   (it has three minutes to flush its state) and recreated on the new image.
3. If the release notes say the version needs a replay, set
   `HIVED_EXTRA_ARGS=--force-replay` in `.env` before step 2 and clear it after
   the node is back. This discards the state and account history and rebuilds
   both from the block log already on disk; nothing is downloaded again.

The same `--force-replay` step is needed after changing the tracked accounts in
`config.ini`. For a release that needs a replay, consider running the upgrade on a
second machine and switching over once it has caught up, to avoid downtime.

A snapshot-only node has no block log to replay from. When a release needs a replay,
or the tracked accounts change, it starts over from a fresh snapshot: stop the
stack, delete the state volume and the unpacked snapshot, point `SNAPSHOT_URL`
at a snapshot made by the new version, and start again:

```
docker compose down
docker volume rm hived-exchange_hived-shm
docker compose run --rm --entrypoint rm hived -rf /home/hived/datadir/snapshot
docker compose up -d
```

## Troubleshooting

- `docker compose logs hived` shows hived's output; the same log is written to
  `hived.log` inside the data volume.
- A container that keeps restarting has a startup error in its log, most often a
  mistake in `config.ini`.
- `unhealthy` with `still syncing` in `docker inspect` output is normal until the
  node has caught up. Raise `HIVED_HEALTHCHECK_MAX_BLOCK_AGE` if your monitoring
  is stricter than the network's block interval warrants.
- `Block ... has been pruned` from `get_block` means the node was bootstrapped
  from a snapshot and only holds its head block; use account history calls, or
  bootstrap from a block log if your integration scans blocks.
- `docker compose down -v` deletes the volumes, including the block log. Plain
  `docker compose down` keeps them.

## Migrating from the script-based setup

If you ran the node with `run_hived_img.sh` or
`build_and_setup_exchange_instance.sh`, its data directory can be reused: bind
`hived-data` to it as described above (and `hived-shm` to the state directory if
that was separate), and copy your tracked-account lines into `config.ini`. The
`config.ini` that lives inside the old data directory is no longer read.

The scripts left the block log in the split layout that hived now uses by
default. This directory's `config.ini` sets `block-log-split = -1` because the
published block log is a single file; when migrating a split block log, remove
that line.
