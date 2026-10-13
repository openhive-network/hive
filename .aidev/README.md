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

| Slot | Steps |
|---|---|
| quick | build, chain-smoke, plugin-api |
| full, baseline | build, then chain-tests and plugin-tests at the same time |
| canary | build, chain-smoke, plugin-api |

`plugin_test` suites other than `witness_tests` are not run singly: `snapshots_tests` fails
when run without the suites before it (it passes in the full `!witness_tests` order, as in CI).
`witness_tests` cases are: each gets its own data directory (`HIVE_TEMPDIR`) and its p2p
node listens on no fixed port.

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
(`chain_test:basic_tests`, `plugin_test:!witness_tests`). Each unit that ends on its
own, passed or failed, appends
`{"action": …, "duration_ms": …, "default_ms": …}` to `AIDEV_TIMEOUTS_OBSERVATIONS`,
from which aidev models the next run's timeouts; a unit killed at its timeout or by
a signal appends nothing, because its duration only says it took longer.
`aidev project preflight` reports `adaptive test timeouts: enabled` in its slots row.

Not covered yet: the Python functional tests (test-tools; several need the 5M
block-log fixtures), fork/colony/mirrornet jobs, replay, and `static`. They stay in
GitLab CI.

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
