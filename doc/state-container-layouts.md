# Shared memory state laid out by another build

`shared_memory.bin` stores a record of each container's layout next to that container. This covers every index and the memory comment archive. A record holds the container's size and a hash of its full type name, as the build that created the state lays them out. On every start hived compares the record of each index and of the archive with the running build before it reads that container. When a record is missing or differs, hived refuses the state instead of reading it through the wrong layout.

For the memory comment archive, the recorded size is the sum of the sizes of the archive object and of the container it holds, and its type hash covers both types; the refusal labels it "sum of the sizes of the container and its members".

## What the records do not cover

Two objects in `shared_memory.bin` have no layout record and are read before any record is compared:

- "environment", which chainbase reads when it opens the file. It holds the compiler version and build type (debug or not) of the build that created the state, the hived version, the plugins and the serialized state definitions; a build with another compiler version or build type refuses the state with "Different persistent & runtime environments". A change to the layout of this object itself is not detected.
- "irreversible", the last irreversible block, which hived reads right after opening the file and before it adds any index. A change to its layout is not detected either.

A build that changes the layout of either of them has to refuse older state by other means.

## State whose first open did not complete

hived creates `shared_memory.bin` with a "creation in progress" mark, stored together with the environment before the file appears under its name, and removes the mark as the last step of the first open: after every index and the comment archive are stored, the state definitions and blockchain configuration are saved and the genesis state is built. A start that finds the mark refuses the state before it grows the file to a larger `shared-file-size` or reads anything else from it (the version, the plugins, any index or the comment archive), with code 78 and the hint below, logging:

```
The state in `shared_memory_file` was not completely created: its first open did not complete (it was interrupted or failed). A replay (or a snapshot load) is needed.
```

So whenever a first open does not complete, the next start exits with code 78, and so does every start after it, until the state is rebuilt with `--force-replay` or replaced with `--load-snapshot=<name>`. That holds whether the process was killed or the open failed: a block log that cannot be opened, a comment archive that cannot be opened or a full disk during the construction of the indexes leaves the mark too, so such state is refused even after the cause is fixed. hived does not wipe such state itself.

A process killed while it changes the state (inside an index construction, for example) can leave the lock of the shared memory segment held. No live process can hold it when hived opens the file, because hived locks the file first, so the lock was left by a killed process. hived refuses that state the same way, with code 78, logging:

```
The lock of `shared_memory_file` was left held by a process that was killed while it changed the state, so the state may be inconsistent. A replay (or a snapshot load) is needed.
```

Only the lock of the segment manager, which every allocation from the segment takes, is checked. The pools that allocate from the segment for some containers keep locks of their own in the file, which are not checked: the memory comment archive (`comment-archive=MEMORY`) allocates its comments from such a pool. A process killed inside an allocation from that pool leaves state that passes this check, and a later start fails when it takes that lock (rewinding the undo state, for example) and exits with code 255 rather than 78.

Both checks read the file through a private mapping, so neither refusal changes anything in the file, the lock included: later starts find the same lock left held and are refused the same way. The refusals in the other sections come later in the open, after the open has grown the file to a larger `shared-file-size` and cleared the environment's `created_storage` flag (one byte) of state never reopened since its creation. So the first such refusal of that state changes that byte (and the size of the file, if it grew), and later ones change nothing.

State created by builds that did not store the mark carries none, so an interrupted first open of such state is caught only by the checks below.

### File systems without hard links

The mark is stored before the file appears under its name only where the file system supports hard links: hived creates the file under a temporary name and links it in place once the mark and the environment are stored. Where hard links are unavailable (hived logs "Hard links are unavailable under ..." once), the file is created under its final name, and hived locks it and stores the mark and the environment only after that. A process killed in between leaves a file without the mark and without the environment, and the next start fails with "Unable to find environment data saved in persistent storage" (or with "the shared memory file holds no complete storage", if the process was killed before the segment was set up in the file) and exits with code 255 rather than 78. That state has to be removed or rebuilt (`--force-replay`) the same way.

## State that only part of the indexes were stored in

When a first open of new state by a build that did not store the creation mark (see [above](#state-whose-first-open-did-not-complete)) is interrupted after some indexes are stored but before the others are, or the state was created by a build that adds other indexes, the next start finds some of the indexes the running build adds and not others. hived refuses that state the same way, with code 78, logging one of:

```
The index <index> is not stored in `shared_memory_file`, but indexes added before it are: the first open of the state was interrupted, or a build that adds other indexes created it. A replay (or a snapshot load) is needed.
The index <index> is stored in `shared_memory_file`, but indexes added before it are not: a build that adds other indexes created the state. A replay (or a snapshot load) is needed.
```

A layout record left without its container, as an open interrupted between storing the record and constructing the container leaves it, does not count as a stored container: the first message reports the container as not stored, whatever layout the record holds. When none of the indexes is stored yet, the open constructs them, replacing such a record. This happens only for state without the creation mark, because state with the mark is refused first.

## The first start after upgrading

Builds before this check did not write layout records, so the first start of an upgraded hived refuses every existing `shared_memory.bin`. The state has to be rebuilt once, by either:

- `--force-replay`, which rebuilds the state from the block log, or
- `--load-snapshot=<name>` (state_snapshot plugin), which replaces it with a snapshot taken by the new build.

State created by the new build (by the replay or the snapshot load) opens normally on later starts.

State written with `comment-archive=MEMORY` by the first build that recorded layouts is refused too, as "differs from the running build": the record of the memory comment archive has covered the type of the container it holds since then, so its recorded hash changed. Such state needs the same replay or snapshot load. State written with the RocksDB comment archive is not affected.

## What the refusal looks like

hived logs one of:

```
The layout of <container> is not recorded in `shared_memory_file`: it was created by a build that does not record container layouts. A replay (or a snapshot load) is needed.
The layout of <container> in `shared_memory_file` {size: ..., type hash: ...} differs from the one of the running build {size: ..., type hash: ...} (the container type or the Boost version changed). A replay (or a snapshot load) is needed.
```

(for the memory comment archive the size is labelled `sum of the sizes of the container and its members`), or one of the messages in [State whose first open did not complete](#state-whose-first-open-did-not-complete) and [State that only part of the indexes were stored in](#state-that-only-part-of-the-indexes-were-stored-in),

followed by

```
The existing state cannot be opened by this version of hived, and it is not wiped automatically. Start hived with --load-snapshot=<snapshot> to replace it with a snapshot, or with --force-replay to rebuild it from the block log.
hived refused to start; a restart will not change that. Exiting with code 78.
```

and exits with code 78 (`STARTUP_REFUSED_EXIT_CODE`). Apart from the changes described in [State whose first open did not complete](#state-whose-first-open-did-not-complete), the state is left untouched. Restarting without one of the options above is refused the same way, so a supervisor should not restart hived on exit code 78. For systemd, use `RestartPreventExitStatus=78`. With Docker, prefer `restart: on-failure` with a retry limit over `restart: always`.

A "differs from the running build" refusal does not only follow a change to hive's own state objects. The type names and sizes of the containers include Boost.MultiIndex and Boost.Interprocess types, so a build against another Boost version can refuse state written by the previous one. That state needs the same replay or snapshot load.

`shared_memory_file_util` stops with the same message when it reads such state.
