# hive under AIDEV

AIDEV verifies changes to this repository through the slots in `project.yaml`,
integrates them into `aidev/integration`, and people merge that into `develop`
through merge requests (as in hive/haf), **keeping the source branch**
(`aidev/integration` is protected). GitLab CI doesn't run for AIDEV branches
(`ai/*`, `session/*`, pushes to `aidev/integration`); see `.gitlab-ci.yaml`
`workflow:`. `develop` requires a successful pipeline, so start one on
`aidev/integration` (web or API) before merging its MR.

## Suites

`.aidev/run-checks.sh <suite> <step>[,<step>...]...` runs the named steps and writes
`test-results/<suite>/junit.xml`, one test case per step, plus each test
binary's own junit report and `timings.tsv`, each step's wall time (also printed
at the end of the output). Steps joined by commas run at the same time and share
`HIVE_TEST_JOBS` test processes (default: min(8, nproc)) evenly; when one ends, the
others take over its share (the step's `run_chain_tests_parallel.sh` reads its
parallelism from a file GNU parallel re-reads as each unit ends), and the log says
`== <step>: now N test processes`. On INT or TERM (the slot wall) the script stops
every step still running and the test processes under it.

| Step | What |
|---|---|
| `build` | configure and build the testnet `chain_test` and `plugin_test` with CI's cmake arguments |
| `chain-smoke` | `chain_test --run_test=basic_tests` (includes the state-definition tests) |
| `chain-tests` | every `chain_test` suite in parallel (`scripts/run_chain_tests_parallel.sh`, CI's `chain_test`) |
| `plugin-api` | `plugin_test` without `witness_tests` (CI's `plugin_test_api`) |
| `plugin-tests` | all of `plugin_test` (CI's `plugin_test_api` + `plugin_test_witness`) in parallel: the suites other than `witness_tests` as one process, each `witness_tests` case as its own |
| `integration` | off unless `HIVE_AIDEV_INTEGRATION=1`; the `tests/integration` scripts the diff selects, each against a hived image built from the checkout (below) |
| `hived-testnet`, `hived-mainnet` | `hived`, `cli_wallet` and `shared_memory_file_util` with CI's arguments, into `<report dir>/bin` (what `integration` builds an image from) |
| `wallet-patterns` | the cli_wallet Python pattern tests (CI's `python_pattern_testnet_tests`) against the testnet `hived`, `cli_wallet` and `get_dev_key` it builds as `hived-testnet` does (below) |

| Slot | Steps |
|---|---|
| quick | build, chain-smoke, plugin-api |
| full | suite `default`: build, then chain-tests and plugin-tests at the same time; suite `integration`: integration (off; below); suite `wallet_patterns`: wallet-patterns, when the diff reaches the wallet (below) |
| baseline | build, then chain-tests and plugin-tests at the same time |
| canary | build, chain-smoke, plugin-api |

Each suite of `full` and `baseline` declares `memory:`, what AIDEV reserves on the
host for one run until it has measured the suite's own runs (the suite's container
is capped a headroom above it, 1.25× by default): `full`'s `default` and `baseline`
40g, `full`'s `integration` 12g (52g together), `full`'s `wallet_patterns` 16g (after
`default`, never beside it). `quick`, `canary` and `coverage`
declare none. Source: over 14 `run_full_tests` runs
(2026-10-13/14, before the integration suite was on), the pull host's free memory
fell by 6-14 GB on steem-11 (32 threads) and up to 35 GB on steem-3 and steem-5
(48 threads), host-wide samples ~30 s apart. The peak is the build, `ninja -j nproc`
uncapped, so it grows with the host's threads. `integration` was not measured: it
is estimated as one hived build at a time (`hived-testnet`, `hived-mainnet`, mostly
compiler-cache hits, then the hived link) and then the scripts' nodes (a 5.1M-block
mainnet sync, testnet nodes at 128M `shared-file-size`). Its builds and nodes are
the host daemon's containers, outside the suite's, so AIDEV never measures them
and this figure stays its price.

`plugin_test` suites other than `witness_tests` are not run singly: `snapshots_tests` fails
when run without the suites before it (it passes in the full `!witness_tests` order, as in CI).
`witness_tests` cases are: each gets its own data directory (`HIVE_TEMPDIR`) and its p2p
node listens on no fixed port.

## Wallet pattern tests

`tests/python/api_tests/python_patterns_tests/cli_wallet_tests` compares the
cli_wallet's `help`, `gethelp` and command outputs with stored patterns, so a
change to the wallet's API surface that leaves them behind fails there. The
`full` slot's `wallet_patterns` suite runs them as CI's
`python_pattern_testnet_tests` does: pytest with `-m "not testnet and not
mainnet_5m and not live_mainnet"`, 600 s per test and 720 s in all, one xdist
worker per `HIVE_TEST_JOBS`, junit in
`test-results/full-wallet-patterns/wallet-patterns.xml`.

- **Selected by the diff.** Its `select`, `.aidev/wallet-patterns-select.sh`, runs
  host-side before the suite (as `integration-select.sh` does, with the same diff
  base) and exits 77, the suite skipped and adding no time, unless the diff changes
  `libraries/wallet/`, `programs/cli_wallet/`, `libraries/protocol/` (the operation
  shapes the wallet prints) or the patterns; else it prints the paths that selected
  it. `.aidev/tests/wallet_patterns_select_test.sh` tests it in a scratch repository.
- **Its own build.** A suite's container keeps nothing of another's, so the step
  builds `hived`, `cli_wallet` and `get_dev_key` (the `hived-testnet` targets and
  the key tool, with CI's testnet cmake arguments and the same `/home/hived` paths)
  and finds `default`'s objects in the compiler cache (380 of 429 compiles hit on
  steem-5: ~4 min, then ~1 min of tests). It needs the stack's relay for that
  cache, so it runs after `default` rather than `concurrent`ly; its 900 s timeout
  and `default`'s 1500 s share `run_full_tests`' 2400 s.
- **The test-tools environment is the image's.** The suite has no network, so
  `.aidev/runtime/Dockerfile` installs, into `/opt/test-tools-venv`, the main group
  of `tests/python/hive-local-tools/poetry.lock` (test-tools, wax, beekeepy,
  hiveio-api, pytest and its plugins), as CI's test jobs do; `hive_local_tools`
  and `tests_api` come from the checkout through `PYTHONPATH`. The lock is an
  image input, so a lock move rebuilds the image. CI also installs the
  `hiveio-api` wheel built from the commit; the image has the lock's. Where `$HIVE_TEST_TOOLS_VENV/bin/python3`
  (`/opt/test-tools-venv` by default) cannot import test-tools, pytest-xdist and
  pytest-timeout, the step builds nothing and is skipped as
  `test_tools_unavailable`, its log the import error, and the suite exits 77.
- **Out of the checkout.** test-tools writes each module's nodes and wallets in a
  `generated_during_*` directory beside it, so the step copies the tests (with
  `tests/python`'s `pytest.ini` and `conftest.py`) to
  `test-results/full-wallet-patterns/wallet-patterns/` and runs them there; the
  nodes' logs stay in that copy.

To run it by hand, in the checks' image as in "The compiler cache" below:
`.aidev/run-checks.sh full-wallet-patterns wallet-patterns`.

## Integration scripts

The docker-based scripts in `tests/integration/` run in GitLab CI, and in the
`full` slot's `integration` suite when the diff reaches them. The step lives in
`.aidev/integration.sh`; `.aidev/tests/integration_test.sh` tests its decisions
(host paths over mountinfo fixtures, the time budget, the build plan, the
Dockerfile-stage check, the diff base) and the selector's without docker, and,
where a docker daemon and the checks' image are at hand, that the image build's
output is handed back to the invoking user however its container ends.

### On, selected before admission, beside `default`

The suite declares `HIVE_AIDEV_INTEGRATION: "1"` and `docker: true` (a protected
sandbox setting, in effect once a maintainer approves it). With
`HIVE_AIDEV_INTEGRATION` not 1 the step does nothing but record one skipped
case, `off`.

AIDEV admits a `docker: true` suite into a dev docker slot (`sandbox.dev`'s
memory, 5.5 GB when undeclared; waiting up to 900 s on a busy host) before its
command runs. So the suite's `select`, `.aidev/integration-select.sh`, runs
first, host-side in AIDEV's worker (bash, git, coreutils/grep/sed and the
docker CLI only, no container started, nothing built or written): it exits 77,
the suite skipped with its last line as the reason, when the step is off, when
there is no diff base, or when the diff selects no script the step can run here
(one needing CI-only fixtures does not count); and, as
`skipped: <reason> (selected: <scripts>)`, when no docker daemon answers, there
is no compose plugin, or the time plan (below) does not fit the suite's
`HIVE_INTEGRATION_BUDGET` for any of them. Else 0, printing the plan, the
scripts it defers and the ones it runs. Most diffs
select none and so wait for no docker slot.

The suite is `concurrent: true`: when selected, it runs beside `default`'s
build, chain_test and plugin_test instead of after them. Its own hived images
build in a separate container while `default` builds, so the two compiles share
the CPU; the scripts mostly wait on docker and block production and overlap well.

Each build's ninja runs `min(nproc, usable memory / HIVE_BUILD_JOB_MEMORY_MB)`
jobs (at least 1; 2048 MB per job by default), the usable memory being the
container's cgroup limit less its usage, else the host's `MemAvailable`, and
`-l nproc`; the step's log says `build: N jobs (...)`. `HIVE_BUILD_JOBS`
overrides the count (`.aidev/build-jobs.sh`, tested by
`.aidev/tests/build_jobs_test.sh`).

AIDEV caps, deprioritises and reaps only the suite's own container; the step's
builds and the scripts' nodes are the host daemon's. So the step, and the scripts
in their runner, call docker through `.aidev/docker-shim/docker`: with
`AIDEV_ENV_CONTAINER_OWNER` set, every container they start carries the label
`aidev.env-container.owner` with that owner, and `--oom-score-adj 900`, and every
image they build the label, which its containers inherit. `docker compose` starts
its containers through the daemon's API, not `docker run`: the services of
`test_snapshot_bootstrap.sh` get the label and OOM score from its generated
`compose.test.yml` instead. The binaries' build container is capped at
`AIDEV_ENV_CONTAINER_MEMORY_BYTES` when set, memory and swap together
(`--memory-swap` equal to `--memory`), and gets `HIVE_BUILD_JOBS`. Without these variables nothing changes.

To run the selector by hand: `HIVE_AIDEV_INTEGRATION=1 .aidev/integration-select.sh`;
the step, on a host with docker:
`HIVE_AIDEV_INTEGRATION=1 .aidev/run-checks.sh full-integration integration`.

### Selection

`integration_table` maps changed paths to scripts: `docker/`, the `Dockerfile`,
the scripts the image and its runtime stages use select
`test_snapshot_bootstrap.sh` and `test_exchange_node_block_log_pruning.sh`; the
exchange instance's scripts select the latter; `libraries/chainbase/`, and a
change to `database.cpp`/`database.hpp` whose lines or enclosing function name
`open`/`resize`, select `test_snapshot_bootstrap.sh`; a changed
`tests/integration/test_*.sh` selects itself. The diff is taken from
`AIDEV_BASE_REF` (the gate's base, which AIDEV sets for this project's suites),
else `AIDEV_IMPLEMENT_BASE_SHA`, else the merge base with
`origin/aidev/integration`; the step prints which, as
`diff base: AIDEV_BASE_REF=<sha>`.

### Skips

A skip records a skipped case naming its reason and never fails the slot. The
step repeats select's checks when it runs; when it ends having run none of the
selected scripts (and none failed), the suite exits 77, its last line `skipped: `
and the skipped cases' reasons, so it is never reported passed for running
nothing (`run-checks.sh` exits 77 for any suite whose steps were all skipped).
When some scripts ran, they decide the verdict; every skipped case's reason is
listed at the end of the output either way.

| Reason | When |
|---|---|
| `off` | `HIVE_AIDEV_INTEGRATION` is not 1 |
| no diff base / selects no script | nothing to run (no time spent) |
| `docker_capability_unavailable` | no docker daemon reachable |
| `docker_compose_unavailable` | no compose plugin on this host |
| `sccache_unreachable` | CI's sccache Redis does not answer and the time left cannot fit the cold builds as measured on this host and a script |
| `out_of_time` | the time left of the budget cannot fit the builds as measured on this host and a script, or an image build or a script reached the deadline |
| `docker_paths_unshared` | the checkout's path on the daemon's host cannot be resolved, or a container given it does not see the checkout |
| `instance_base_unpublished` | the registry's `minimal-runtime:${CI_IMAGE_TAG}` cannot be pulled |
| needs fixtures only CI has | a selected script other than the two above |

### Time

AIDEV hands a suite no deadline, so the step keeps its own: it ends by the
earlier of `HIVE_INTEGRATION_BUDGET` (2100 s, the suite's `timeout`: the
40 min `run_full_tests` activity less the 300 s a suite's wall keeps for it) from its
start and `HIVE_INTEGRATION_ACTIVITY_SECONDS` (2400 s, `run_full_tests`'
start-to-close) from when the slot's first suite started (its
`test-results/full/started-at`; `default` starts with this suite, so when it has
not written that file yet the step's own start stands in, and an earlier run's
file still there can only make the deadline earlier), less
`HIVE_INTEGRATION_MARGIN` (300 s). Each
script gets CI's timeout (2100 s, 1800 s) or what is left, whichever is shorter,
and is not started with less than it needs at least (300 s, 600 s). A script
stopped at CI's timeout fails; one stopped at the deadline is `out_of_time`.
The script is given the epoch second it is stopped at as
`HIVE_INTEGRATION_DEADLINE`, which `tests/integration/conftest.sh` takes as its
steps' ceiling (less 60 s), ending with `timeout`'s status, 124, when a step
reaches it; a step also fails once it makes no progress for
`HIVE_INTEGRATION_STALL_SECONDS` (300 s). `tests/integration/conftest_test.sh`
tests those helpers against stub containers.

The plan (select's and the step's, printed as `plan: ...`) takes, for each
image, what its step (`integration-image-<network>`) last took on this host with
the compiler cache as it is now (warm or cold), and each selected script's least
time. The images build one after another and the scripts then run side by side,
so a plan fits when its builds and its longest least time do. Scripts join the
plan the cheapest first: one whose image is already planned before one adding a
build, then the one the plan ends soonest with. A script that does not fit is
deferred, reported as the skipped case `integration-<script>` with
`deferred: out_of_time: ...`, and its image is not built; when none fits the
suite is skipped. The step keeps that measurement on the docker daemon, as
the name of a volume labelled `hive.integration.build=<network>-<cache>`
(`hive-integration-build-testnet-warm-412`), because the run's checkout and
results go with it: a build's time when it builds; when the deadline cuts it
off, that it takes longer than it ran (`...-gt900`), unless a longer build was
already measured. A build not measured on this host yet is planned at nothing,
so the run tries it and measures it, bounded by its deadline.

### Images

For each network a selected script needs (testnet for the snapshot bootstrap,
mainnet for the exchange node), the step builds the binaries in the checks'
image with CI's paths and cmake arguments on the host's network, where sccache
uses CI's Redis if it answers and a local cache otherwise; then the
`Dockerfile`'s `instance` stage around them (its `build` stage replaced through
`--build-context`). Its base is the registry's `minimal-runtime:${CI_IMAGE_TAG}`,
unless the diff changes a line above the `build` stage or a file the runtime
stages add (`scripts/setup_ubuntu.sh`, `scripts/openssl.conf`): then the
`minimal-runtime` stage is built from the change.

### Paths and runs

The scripts bind-mount their work directories, and a worker's paths are not the
daemon host's. The step reads the daemon's mount table (through a container in
the host's PID namespace), maps the checkout from its own mount (the longest
mount point holding it: device and path within it) to the host's mount of that
device, and checks that a container given the result sees the checkout. The
scripts run side by side in the checks' image plus this host's compose plugin,
with the checkout at its host path; one run per script per host at a time (their
container names are fixed), held by a `hive-integration-lock-<script>` docker
network. `test-results/full-integration/` gets one junit case per image and per
script, named `integration-<script>`, and each script's `*.log` files; the rest of
its work directory is deleted.

## Test unit timeouts

`scripts/run_chain_tests_parallel.sh` runs each unit under its own `timeout -k 30`:
600 s for a `chain_test` unit, 900 s for a `plugin_test` one (`colony_basic_test`
takes ~450 s on a quiet host). These are floors. The profile opts into AIDEV's
adaptive timeouts (`timeouts: {enabled: true}`; aidev's
`docs/operations/adaptive-timeouts.md`), so before each suite aidev writes
`AIDEV_TIMEOUTS_FILE`:

```json
{"version": 1, "scale": 1.4, "actions": {"plugin_test:witness_tests/colony_basic_test": {"timeout_ms": 1260000}}}
```

The script's `aidev_timeout_ms <action> <default_ms>` gives the action's `timeout_ms`
if it has one, else `default_ms × scale`, never less than `default_ms`; without the
file, or with one it cannot read, `default_ms`, so CI and runs by hand behave as
before. An action is `<binary>:<unit>`, the unit as passed to `--run_test`
(`chain_test:basic_tests`, `plugin_test:!witness_tests`). Each unit that completes
without a crash, passed (exit 0) or failed only its checks (201 with no `<error>`
entry in its JUnit log), appends
`{"action": …, "duration_ms": …, "default_ms": …}` to `AIDEV_TIMEOUTS_OBSERVATIONS`,
from which aidev models the next run's timeouts. A unit killed at its timeout or by
a signal, one with an exception, a fatal error or a crash Boost.Test caught (201
with an `<error>` entry), or one whose setup failed (200), appends nothing, because
its duration says nothing of how long the unit takes. A unit killed at its timeout is a failed testcase, named after the unit, in
the binary's JUnit results.
`aidev project preflight` reports `adaptive test timeouts: enabled` in its slots row.

Not covered yet: the Python functional tests other than the cli_wallet pattern
tests (test-tools; several need the 5M block-log fixtures), fork/colony/mirrornet jobs, replay, the integration scripts
that need CI's fixtures, and `static`. They stay in GitLab CI.

## The compiler cache

Nothing runs without compiling hived, which is 30-60 minutes cold. The checks share
CI's sccache Redis (`SCCACHE_REDIS` in `.gitlab-ci.yaml`):

- the suite container has no network of its own, so the profile declares a
  one-service stack, `sccache-relay.compose.yml`, that relays `sccache:6379` to CI's
  Redis;
- sccache keys include absolute paths, so `run-checks.sh` builds from
  `/home/hived/source` into `/home/hived/build`, with the cmake arguments of CI's
  `testnet_node_build` (the `Dockerfile`'s build stage, precompiled headers off),
  in CI's own build image. A check then hits what CI
  compiled, and CI hits what a check compiled (466 of 468 compiles on the first
  local run; build 2-3 minutes). `/home/hived/source` is a container-local mirror
  of the checkout: links to its entries, plus a `.git` holding HEAD as a detached
  hash and links that make the submodules' relative `gitdir:` paths resolve,
  because fc's revision CMake reads those files itself and cannot follow a
  worktree's (or an absolute) git directory;
- when the relay or Redis is unreachable, sccache falls back to a local cache and
  the build is cold. `test-results/<suite>/build.log` ends with `sccache --show-stats`.

To run by hand (from a clone with submodules; a worktree also needs its git
directory mounted at the same path and the checkout mounted at its own path):

```bash
docker network create hive-checks
docker run -d --rm --name hive-sccache --network hive-checks --network-alias sccache \
    alpine/socat:1.8.0.3 TCP-LISTEN:6379,fork,reuseaddr TCP:192.168.50.109:6379
docker run --rm --network hive-checks --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
    "$(grep -o 'registry[^"]*aidev-tests@sha256:[0-9a-f]*' .aidev/project.yaml)" \
    .aidev/run-checks.sh quick build chain-smoke plugin-api
```

## The test runtime image (`runtime/`)

CI's build image, `ci-base-image`, with its entrypoint cleared and the main group
of `tests/python/hive-local-tools/poetry.lock` installed in `/opt/test-tools-venv`.
`project.yaml`'s `environment.base` follows the `ci-base-image` tag CI uses
(`CI_BASE_IMAGE_TAG` in `scripts/ci-helpers/ci_image_tag_vars.yml`; keep the two
equal): before each run AIDEV resolves that tag to its digest and builds the
Dockerfile on it (context `tests/python/hive-local-tools`), rebuilding when the tag
moves or the Dockerfile, `pyproject.toml` or `poetry.lock` changes, so none of
them needs a re-pin commit. `environment.image` is only the seed pin AIDEV reads
before a slot resolves (and the image the snippet above and `integration.sh`
name); it does not have to move. `build.sh` builds and pushes a seed when one is
needed:

```bash
.aidev/runtime/build.sh --push   # registry digest if aidev-<input hash> exists, else build + push
# put the printed repo@sha256:<digest> into project.yaml environment.image
```
