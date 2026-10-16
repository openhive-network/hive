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

| Slot | Steps |
|---|---|
| quick | build, chain-smoke, plugin-api |
| full | suite `default`: build, then chain-tests and plugin-tests at the same time; suite `integration`: integration (off; below) |
| baseline | build, then chain-tests and plugin-tests at the same time |
| canary | build, chain-smoke, plugin-api |

Each suite of `full` and `baseline` declares `memory:`, what AIDEV reserves on the
host for one run until it has measured the suite's own runs (the suite's container
is capped a headroom above it, 1.25× by default): `full`'s `default` and `baseline`
40g, `full`'s `integration` 12g (52g together). `quick`, `canary` and `coverage`
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

## Integration scripts

The docker-based scripts in `tests/integration/` run in GitLab CI, and in the
`full` slot's `integration` suite when the diff reaches them. The step lives in
`.aidev/integration.sh`; `.aidev/tests/integration_test.sh` tests its decisions
(host paths over mountinfo fixtures, the time budget, the build plan, the
Dockerfile-stage check, the diff base) and the selector's without docker.

### On, selected before admission, beside `default`

The suite declares `HIVE_AIDEV_INTEGRATION: "1"` and `docker: true` (a protected
sandbox setting, in effect once a maintainer approves it). With
`HIVE_AIDEV_INTEGRATION` not 1 the step does nothing but record one skipped
case, `off`.

AIDEV admits a `docker: true` suite into a dev docker slot (`sandbox.dev`'s
memory, 5.5 GB when undeclared; waiting up to 900 s on a busy host) before its
command runs. So the suite's `select`, `.aidev/integration-select.sh`, runs
first, host-side in AIDEV's worker (bash, git and coreutils/grep/sed only,
nothing built or written): it exits 77, the suite skipped with its last line as
the reason, when the step is off, when there is no diff base, or when the diff
selects no script the step can run here (one needing CI-only fixtures does not
count); else 0, printing the scripts. Most diffs select none and so wait for no
docker slot.

The suite is `concurrent: true`: when selected, it runs beside `default`'s
build, chain_test and plugin_test instead of after them. Its own hived images
build (`ninja -j nproc`) in a separate container while `default` builds, so the
two compiles share the CPU; the scripts mostly wait on docker and block
production and overlap well. `HIVE_BUILD_JOBS` is left uncapped.

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

A skip records a skipped case naming its reason and never fails the slot:

| Reason | When |
|---|---|
| `off` | `HIVE_AIDEV_INTEGRATION` is not 1 |
| no diff base / selects no script | nothing to run (no time spent) |
| `docker_capability_unavailable` | no docker daemon reachable |
| `docker_compose_unavailable` | no compose plugin on this host |
| `sccache_unreachable` | CI's sccache Redis does not answer and the time left cannot fit cold builds (`HIVE_INTEGRATION_COLD_BUILD_SECONDS`, 2700 s per image) |
| `out_of_time` | the time left cannot fit the builds (300 s each on the cache) and a script, or an image build or a script reached the deadline |
| `docker_paths_unshared` | the checkout's path on the daemon's host cannot be resolved, or a container given it does not see the checkout |
| `instance_base_unpublished` | the registry's `minimal-runtime:${CI_IMAGE_TAG}` cannot be pulled |
| needs fixtures only CI has | a selected script other than the two above |

### Time

AIDEV hands a suite no deadline, so the step keeps its own: it ends by the
earlier of `HIVE_INTEGRATION_BUDGET` (1200 s, the suite's `timeout`) from its
start and `HIVE_INTEGRATION_ACTIVITY_SECONDS` (2400 s, `run_full_tests`'
start-to-close) from when the slot's first suite started (its
`test-results/full/started-at`; `default` starts with this suite, so when it has
not written that file yet the step's own start stands in, and an earlier run's
file still there can only make the deadline earlier), less
`HIVE_INTEGRATION_MARGIN` (300 s). Each
script gets CI's timeout (2100 s, 1800 s) or what is left, whichever is shorter,
and is not started with less than it needs at least (300 s, 600 s). A script
stopped at CI's timeout fails; one stopped at the deadline is `out_of_time`.

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

Not covered yet: the Python functional tests (test-tools; several need the 5M
block-log fixtures), fork/colony/mirrornet jobs, replay, the integration scripts
that need CI's fixtures, and `static`. They stay in GitLab CI.

## The compiler cache

Nothing runs without compiling hived, which is 30-60 minutes cold. The checks share
CI's sccache Redis (`SCCACHE_REDIS` in `.gitlab-ci.yaml`):

- the suite container has no network of its own, so the profile declares a
  one-service stack, `sccache-relay.compose.yml`, that relays `sccache:6379` to CI's
  Redis;
- sccache keys include absolute paths, so `run-checks.sh` builds from
  `/home/hived/source` into `/home/hived/build`, with the cmake arguments of CI's
  `testnet_node_build`, in CI's own build image. A check then hits what CI
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

CI's build image, `ci-base-image`, pinned by digest, with its entrypoint cleared.
When `CI_BASE_IMAGE_TAG` (`scripts/ci-helpers/ci_image_tag_vars.yml`) moves,
re-pin the `FROM` to the new tag's digest, rebuild and re-pin **in the same commit**:

```bash
.aidev/runtime/build.sh --push   # registry digest if aidev-<input hash> exists, else build + push
# put the printed repo@sha256:<digest> into project.yaml environment.image
```
