# Shared memory state laid out by another build

`shared_memory.bin` stores a record of each container's layout next to that container. This covers every index and the memory comment archive. A record holds the container's size and a hash of its full type name, as the build that created the state lays them out. On every start hived compares these records with the running build before it reads any container. When a record is missing or differs, hived refuses the state instead of reading it through the wrong layout.

## The first start after upgrading

Builds before this check did not write layout records, so the first start of an upgraded hived refuses every existing `shared_memory.bin`. The state has to be rebuilt once, by either:

- `--force-replay`, which rebuilds the state from the block log, or
- `--load-snapshot=<name>` (state_snapshot plugin), which replaces it with a snapshot taken by the new build.

State created by the new build (by the replay or the snapshot load) opens normally on later starts.

## What the refusal looks like

hived logs one of:

```
The layout of <container> is not recorded in `shared_memory_file`: it was created by a build that does not record container layouts. A replay (or a snapshot load) is needed.
The layout of <container> in `shared_memory_file` {size: ..., type hash: ...} differs from the one of the running build {size: ..., type hash: ...} (the container type or the Boost version changed). A replay (or a snapshot load) is needed.
```

followed by

```
The existing state cannot be opened by this version of hived, and it is not wiped automatically. Start hived with --load-snapshot=<snapshot> to replace it with a snapshot, or with --force-replay to rebuild it from the block log.
hived refused to start; a restart will not change that. Exiting with code 78.
```

and exits with code 78 (`STARTUP_REFUSED_EXIT_CODE`). The state is left untouched. Restarting without one of the options above is refused the same way, so a supervisor should not restart hived on exit code 78. For systemd, use `RestartPreventExitStatus=78`. With Docker, prefer `restart: on-failure` with a retry limit over `restart: always`.

A "differs from the running build" refusal does not only follow a change to hive's own state objects. The type names and sizes of the containers include Boost.MultiIndex and Boost.Interprocess types, so a build against another Boost version can refuse state written by the previous one. That state needs the same replay or snapshot load.

`shared_memory_file_util` stops with the same message when it reads such state.
