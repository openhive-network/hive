# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Hive** is a Graphene-based social blockchain forked from Steem. This repository contains the core blockchain node implementation (`hived`), CLI wallet, beekeeper (wallet daemon), and associated libraries.

**Primary Development**: GitLab at https://gitlab.syncad.com/hive/hive (project ID: 198)
**Language**: C++ (C++17 standard) with Python test infrastructure
**Build System**: CMake + Ninja

## Build Commands

### Building from Source (Ubuntu 24.04 LTS)

**Prerequisites:**
```bash
sudo ./scripts/setup_ubuntu.sh --dev
```

**Configure and build:**
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -GNinja ../
ninja

# Build specific targets
ninja hived cli_wallet
```

**Build options:**
- `CMAKE_BUILD_TYPE`: Release (production), RelWithDebInfo (debugging symbols), Debug (full debug)
- `BUILD_HIVE_TESTNET=ON`: Required for building unit tests and testnet nodes
- `HIVE_CONVERTER_BUILD=ON`: MirrorNet configuration (imports mainnet data for testing)

**Memory requirements**: 16GB+ RAM for building

### Docker Build

```bash
# Build docker image
mkdir workdir && cd workdir
../hive/scripts/ci-helpers/build_instance.sh <tag> ../hive <registry>

# Options:
#   --network-type=mainnet|testnet|mirrornet
#   --export-binaries=PATH (extract binaries from image)

# Run image
../hive/scripts/run_hived_img.sh <image:tag> --name=hived-instance --data-dir=<path>
```

## Testing

### C++ Unit Tests

**Build and run:**
```bash
# Configure with testnet build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_HIVE_TESTNET=ON -GNinja ../

# Build unit test target
ninja chain_test

# Run tests
./tests/chain_test
```

**Test categories** (in `tests/unit/tests/`):
- `basic_tests` - Basic functionality
- `block_tests` - Block chain tests
- `operation_tests` - Operation validation
- `operation_time_tests` - Time-based operations (vesting withdrawals, etc.)
- `serialization_tests` - Serialization/deserialization

**Code coverage:**
```bash
cmake -DBUILD_HIVE_TESTNET=ON -DENABLE_COVERAGE_TESTING=true -DCMAKE_BUILD_TYPE=Debug ../
ninja && lcov --capture --initial --directory . --output-file base.info --no-external
./tests/chain_test
lcov --capture --directory . --output-file test.info --no-external
lcov --add-tracefile base.info --add-tracefile test.info --output-file total.info
genhtml total.info --output-directory lcov
```

### Python Functional Tests

**Environment setup:**
```bash
# Set paths to binaries
export HIVE_BIN_ROOT_PATH=/path/to/build
export HIVED_PATH="${HIVE_BIN_ROOT_PATH}/programs/hived/hived"
export CLI_WALLET_PATH="${HIVE_BIN_ROOT_PATH}/programs/cli_wallet/cli_wallet"

# Install test-tools dependencies
cd tests/python/hive-local-tools
poetry install
```

**Running tests:**
```bash
# Run all functional tests
pytest -n auto tests/python/functional/

# Run specific test
pytest tests/python/functional/operation_tests/transfer_tests/test_transfer.py::test_name

# Run with markers
pytest -m testnet tests/python/functional/
pytest -m mainnet_5m tests/python/functional/

# Common flags:
#   -n auto          : Parallel execution (pytest-xdist)
#   --durations=0    : Show test durations
#   -v               : Verbose output
#   -k <pattern>     : Filter tests by name pattern
```

**Test markers** (in `tests/python/pytest.ini`):
- `testnet` - Runs on testnet node
- `mirrornet` - Runs on mirrornet
- `mainnet_5m` - Runs against 5M block mainnet replay
- `replayed_node` - Requires pre-replayed node
- `enable_plugins` - Specifies required plugins

**Running via script:**
```bash
./scripts/run_pytest_tests.sh <test_subdir> <binaries_root_dir> [pytest_args...]

# Example:
./scripts/run_pytest_tests.sh tests/python/functional build/
```

**Environment variables:**
- `TEST_TOOLS_NODE_DEFAULT_WAIT_FOR_LIVE_TIMEOUT`: Node startup timeout (default: 30s)
- `PYTEST_NUMBER_OF_PROCESSES`: Parallel workers (default: 8)

## Dependency Management (Poetry)

The lockfile pins exact versions of all dependencies (direct and transitive). This prevents dependency mismatches between environments - if the lockfile is wrong or missing, builds may fail or behave differently. These rules keep it synchronized with pyproject.toml.

- **Dependency versions are specified in `pyproject.toml` and locked in `poetry.lock`**
- **Always use `poetry lock`** (without additional flags like `--regenerate`)
- **Always run `poetry lock` after changing `pyproject.toml`**
- **The `poetry.lock` file must be in the repository** - never add it to `.gitignore`
- **Never delete `poetry.lock`** - it ensures reproducible builds
- **Never edit `poetry.lock` manually** - always use poetry commands
- **Don't upgrade dependencies on your own** - only upgrade when explicitly requested

## Architecture

### Core Components

**Programs** (in `programs/`):
- `hived/` - Main blockchain node daemon
- `cli_wallet/` - Command-line wallet for transaction signing
- `beekeeper/` - Standalone wallet daemon with HTTP/WebSocket API
- `blockchain_converter/` - Convert block_log between formats

**Libraries** (in `libraries/`):
- `chain/` - Blockchain state machine, consensus logic, evaluators
- `protocol/` - Operation definitions, serialization, base types
- `plugins/` - Modular plugin system (see Plugin System below)
- `fc/` - Fast-compiling C++ library (serialization, crypto, networking)
- `chainbase/` - Memory-mapped database (shared_memory.bin storage)
- `appbase/` - Application framework for plugin-based architecture
- `wallet/` - Wallet logic shared by cli_wallet and beekeeper

### Plugin System

Hive uses an appbase plugin architecture. Plugins are in `libraries/plugins/`:

**Core plugins:**
- `chain` - Blockchain state (required)
- `p2p` - Peer-to-peer networking
- `witness` - Block production for witnesses
- `webserver` - HTTP/WebSocket server for APIs

**API plugins** (in `plugins/apis/`):
- `condenser_api` - Legacy API (Steem-compatible)
- `database_api` - Read blockchain state
- `account_history_api` - Account transaction history (requires account_history_rocksdb)
- `block_api` - Block retrieval
- `wallet_bridge_api` - Wallet operations without managing keys
- `network_broadcast_api` - Transaction broadcasting
- `rc_api` - Resource Credits API
- `market_history_api` - DEX market data

**Data plugins:**
- `account_history_rocksdb` - Indexes account history in RocksDB
- `account_by_key` - Public key → account lookup
- `market_history` - Market/trading history
- `transaction_status` - Transaction status tracking

**Network types:**
- **Mainnet**: Production blockchain (default build)
- **Testnet**: Isolated test network (`BUILD_HIVE_TESTNET=ON`)
- **Mirrornet**: Mainnet clone for testing (`HIVE_CONVERTER_BUILD=ON`)

### Test Infrastructure (test-tools)

Python test infrastructure is in `tests/python/hive-local-tools/test-tools/`. This is a separate library for orchestrating hived nodes, managing testnets, and executing blockchain operations.

**Key abstractions:**
- `tt.InitNode` - Bootstrap node for network initialization
- `tt.WitnessNode` - Block-producing witness node
- `tt.ApiNode` - API-only node (no block production)
- `tt.Network` - Container for multiple interconnected nodes
- `tt.Wallet` - Beekeeper-based wallet for signing transactions
- `tt.Account` - Deterministic key generation from account names
- `tt.Snapshot` - Save/restore node state (avoids replay)

**Scope-based cleanup**: Test resources (nodes/wallets) are automatically cleaned up when tests exit via pytest fixtures.

## Configuration

### Node Configuration

**Generate default config:**
```bash
hived --dump-config  # Creates ~/.hived/config.ini
```

**Docker config example:** `contrib/config-for-docker.ini`

**Critical settings:**
- `plugin` - Plugins to load (e.g., `p2p witness webserver condenser_api`)
- `webserver-http-endpoint` - HTTP API endpoint (e.g., `0.0.0.0:8091`)
- `webserver-ws-endpoint` - WebSocket endpoint
- `shared-file-size` - Shared memory size (~12GB for mainnet, 128MB for testnet)
- `p2p-endpoint` - P2P listening address
- `p2p-seed-node` - Seed nodes for network discovery (see `doc/seednodes.txt`)

**Witness config** (for block production):
- `witness` - Witness account name
- `private-key` - Block signing key (WIF format)

### CLI Wallet Configuration

**Connection:**
```bash
cli_wallet --server-rpc-endpoint=ws://127.0.0.1:8091
```

**Daemon mode** (RPC server):
```bash
cli_wallet --daemon --rpc-http-endpoint=127.0.0.1:8092 --rpc-http-allowip=127.0.0.1
```

**Required node plugins:** `account_by_key_api`, `condenser_api`, `database_api`, `account_history_api`, `wallet_bridge_api`

## Development Patterns

### Adding New Operations

Operations are defined in `libraries/protocol/include/hive/protocol/operations.hpp`. See `doc/devs/create-operation.md` for detailed guide.

**Key files:**
- Operation definition: `libraries/protocol/include/hive/protocol/`
- Evaluator (validation logic): `libraries/chain/include/hive/chain/evaluator.hpp`
- Tests: `tests/unit/tests/operation_tests.cpp`

### Adding New Plugins

Use the plugin generator:
```bash
python3 programs/util/newplugin.py <plugin_name>
```

Creates skeleton in `example_plugins/<plugin_name>/` with CMake setup, plugin class, and API stubs.

### Python Test Development

**Test structure:**
```python
import test_tools as tt

def test_example():
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    # Create account
    wallet.api.create_account('initminer', 'alice', '{}')

    # Wait for blocks
    node.wait_number_of_blocks(3)

    # Query state
    account = node.api.database.find_accounts(['alice'])
    assert account
```

**Important patterns:**
- Use `@run_for("testnet")` decorator to specify node type
- Use `node.wait_for_block_with_number(N)` instead of `time.sleep()` for synchronization
- Create snapshots for expensive setup: `snapshot = node.dump_snapshot()`
- Batch operations: `with wallet.in_single_transaction(): ...`

## CI/CD Pipeline

**CI images and templates** come from `hive/common-ci-configuration`. Key version refs:
- `CI_COMMON_JOB_VERSION` in `scripts/ci-helpers/prepare_data_image_job.yml` - docker-builder/docker-dind tags
- `CI_BASE_IMAGE_TAG` in `scripts/ci-helpers/ci_image_tag_vars.yml` - Python/build environment image

**GitLab CI stages** (`.gitlab-ci.yaml`):
1. `static_code_analysis` - Code quality checks
2. `build` - Build hived docker images (mainnet, testnet, mirrornet)
3. `test` - Run C++ unit tests and Python functional tests
4. `cleanup` - Clean up old cache
5. `publish` - Publish docker images
6. `deploy` - Deploy to infrastructure

**Key jobs:**
- `prepare_hived_image` - Build mainnet docker image
- `testnet_node_build` - Build testnet binaries
- `prepare_hived_data` - Replay to 5M blocks for testing

**CI variables:**
- `PYTEST_NUMBER_OF_PROCESSES=8` - Parallel test workers
- `BLOCK_LOG_SOURCE_DIR_5M=/blockchain/block_log_5m` - Pre-generated 5M block_log
- `DATA_CACHE_HIVE_PREFIX=/nfs/ci-cache/hive/replay_data_hive` - Replay cache location

### CI Skip Optimization

The CI automatically optimizes pipeline execution based on what files changed. This saves significant CI time (~60+ minutes for full pipeline).

**Automatic optimization** (defined in `scripts/ci-helpers/skip_rules.yml`):

| Changed Files | Build Jobs | Test Jobs | Notes |
|--------------|------------|-----------|-------|
| `doc/**`, `*.md` only | Skipped | Skipped | Docs-only changes |
| `tests/**` only | Skipped | Run | Uses cached binaries via `quick_test_setup` |
| `libraries/**`, `programs/**` | Run | Run | Source code changes |
| `scripts/**`, `.gitlab-ci.yaml` | Run | Run | CI/scripts changes |

**How test-only optimization works:**
1. `detect_changes` job analyzes what files changed
2. If only `tests/**` changed (no source code):
   - Queries registry API to verify cached binaries exist
   - If cache exists: sets `TESTS_ONLY=true`, skips builds
   - If no cache: runs full build (self-healing fallback)
3. `quick_test_setup` fetches cached binaries when `TESTS_ONLY=true`
4. Test jobs run using the cached binaries

**Override variables:**
- `FORCE_FULL_PIPELINE=true` - Run all jobs regardless of changes
- `QUICK_TEST=true` - Skip builds, use cached binaries (manual mode)

**Protected branches**: Full pipeline always runs on `develop`, `master`, and tags.

**Files triggering C++ builds:**
- `libraries/**/*`, `programs/**/*` - Source code
- `CMakeLists.txt`, `cmake/**/*` - Build configuration
- `Dockerfile`, `docker/**/*` - Docker configuration
- `scripts/ci-helpers/*.sh` - Build scripts

## Code Style

**C++:**
- C++17 standard required
- Follow existing code conventions (see `doc/git-guidelines.md`)
- Use Boost.Test for unit tests
- No specific formatter enforced (match existing style)

**Python:**
- Format with `ruff format` (line-length: 120) - **REQUIRED before committing**
- Lint with `ruff` (see `tests/python/hive-local-tools/test-tools/pyproject.toml`)
- Type hints with `mypy` in strict mode
- All files require: `from __future__ import annotations`

**Before committing Python:**
```bash
cd tests/python/hive-local-tools
poetry run ruff format .
```

## System Requirements

**Runtime (consensus node):**
- Disk: ~600GB for full block_log
- Memory: ~12GB for shared_memory.bin (state data)
- OS: Ubuntu 24.04 LTS (minimum supported)

**Build:**
- RAM: 16GB minimum
- CPU: Multi-core recommended (Ninja uses all cores by default)
- Disk: ~10GB for build artifacts

## Important Notes

### Submodules

This repo contains several submodules that require coordination:

| Submodule | Path | Repo |
|-----------|------|------|
| npm-common-config | `programs/beekeeper/beekeeper_wasm/npm-common-config` | common-ci-configuration |
| test-tools | `tests/python/hive-local-tools/test-tools` | test-tools |
| tests_api | `tests/python/hive-local-tools/tests_api` | tests-api |

When updating CI configuration, ensure `npm-common-config` submodule matches the ref in `prepare_data_image_job.yml`.

### Test-Tools Dependency

When making changes requiring test-tools updates:
1. Create MR in test-tools repository first
2. Update hive's test-tools submodule to point to your test-tools MR
3. Both CIs must pass before merge
4. Mention test-tools dependency in hive MR description

See CONTRIBUTING.md for details.

### Block Log Paths

- Mainnet 5M: `/blockchain/block_log_5m/` (on CI builders)
- Mirrornet 5M: `/blockchain/block_log_5m_mirrornet/`
- NFS cache: `/nfs/ci-cache/` (shared across CI runners)

**Note**: Symlink block_log files, don't copy (saves time/space).

### API Compatibility

- **condenser_api**: Legacy Steem-compatible API (avoid for new code)
- **database_api**: Modern structured API (preferred)
- Breaking API changes require community discussion and HF planning

### Debugging

**Python test failures:**
- Check node logs: Test framework keeps logs in temp directories
- Use `tt.logger.info()` for debug output
- Run single test with `-v` for verbose output
- Check node process didn't crash: `node.is_running()`

**C++ unit test failures:**
- Run with `--log_level=all` for verbose Boost.Test output
- Use `FC_LOG` macros in code for debugging
- Check shared_memory.bin corruption (delete and retry)

**Network issues:**
- Verify p2p connections: `node.api.network_node.get_connected_peers()`
- Check firewall/network config for p2p ports
- Ensure seed nodes are reachable
