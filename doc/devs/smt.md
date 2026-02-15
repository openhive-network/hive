# Smart Media Tokens (SMT) Developer Guide

This document describes SMT behavior in the current codebase, including:

- SMT-specific operations and objects
- SMT-related behavior in existing core operations
- Database API endpoints for SMT data
- RC charging as currently implemented
- Current implementation caveats

## 1. Enablement and Activation

SMT code is compile-time gated and consensus-time gated:

- Compile-time:
  - `-DENABLE_SMT_SUPPORT=ON` enables `HIVE_ENABLE_SMT` code paths.
  - `-DBUILD_HIVE_TESTNET=ON` defines `IS_TEST_NET` (required by SMT unit tests and commonly used with SMT development).
- Consensus-time:
  - SMT evaluators assert hardfork activation via `HIVE_SMT_HARDFORK`.
  - `HIVE_SMT_HARDFORK` is mapped to `HIVE_HARDFORK_1_29`.

References:

- `cmake/hive_options.cmake`
- `libraries/protocol/hardfork.d/1_29.hf`
- `libraries/chain/smt_evaluator.cpp`

## 2. Symbol and NAI Model

SMTs are represented as NAI symbols (`asset_symbol_type::smt_nai_space`).

- Liquid and vesting SMT symbols are paired by the vesting bit in the symbol.
- NAI pool provides candidate SMT liquid symbols before creation.
- NAI pool entries are replenished and consumed as SMTs are created.

Relevant constants:

- `SMT_MIN_NON_RESERVED_NAI` / `SMT_MAX_NAI`
- `SMT_MAX_NAI_POOL_COUNT`
- `SMT_MAX_NAI_GENERATION_TRIES`

References:

- `libraries/protocol/include/hive/protocol/asset_symbol.hpp`
- `libraries/chain/include/hive/chain/smt_objects/nai_pool_object.hpp`
- `libraries/chain/smt_objects/nai_pool.cpp`
- `libraries/chain/include/hive/chain/util/nai_generator.hpp`

## 3. Core SMT Objects

### `smt_token_object`

Primary SMT record (one object for both liquid and vesting variants):

- Symbol/control:
  - `liquid_symbol`
  - `control_account`
  - `phase`
- Supply/vesting pools:
  - `current_supply`
  - `total_vesting_fund_smt`
  - `total_vesting_shares`
  - `pending_rewarded_vesting_shares`
  - `pending_rewarded_vesting_smt`
- Runtime/setup parameters:
  - voting, windows, regen, reward curves, etc.
- Metadata:
  - `token_name`
  - `token_description`
  - `token_image_url`
  - `token_json_metadata`

Indices:

- `by_id`
- `by_symbol`
- `by_control_account`

### `smt_ico_object`

Stores setup and contribution window/cap data:

- generation policy
- contribution begin/end
- launch time
- soft/hard cap
- cumulative contributed HIVE

Index:

- `by_symbol`

### `smt_token_emissions_object`

Stores scheduled emission configuration:

- schedule time
- interval and count
- destination emission unit
- endpoint interpolation parameters

Index:

- `by_symbol_time`

### `smt_contribution_object`

Stores contribution records:

- `symbol`
- `contributor`
- `contribution_id`
- `contribution` (HIVE)

Indices:

- `by_symbol_contributor`
- `by_symbol_id`
- `by_contributor`

### `smt_allowance_object`

Stores delegated spending approvals:

- `owner`
- `spender`
- `symbol`
- `remaining`

Indices:

- `by_owner_spender`
- `by_spender`

References:

- `libraries/chain/include/hive/chain/smt_objects/smt_token_object.hpp`

## 4. SMT Operation Set

SMT operations in `operation` variant:

- `claim_reward_balance2_operation`
- `smt_create_operation`
- `smt_setup_operation`
- `smt_setup_emissions_operation`
- `smt_set_setup_parameters_operation`
- `smt_set_runtime_parameters_operation`
- `smt_contribute_operation`
- `smt_set_token_metadata_operation`
- `smt_approve_operation`
- `smt_transfer_from_operation`
- `smt_transfer_control_operation`

Reference:

- `libraries/protocol/include/hive/protocol/operations.hpp`

### 4.1 `claim_reward_balance2_operation`

Purpose:

- Claims reward balances from a vector of tokens, including SMT rewards.

Authority:

- Posting (`account`)

Validation highlights:

- `reward_tokens` must be non-empty
- Symbols must be unique and sorted by NAI
- No negative claims
- At least one positive amount

Apply highlights:

- SMT rewards: reward balance is debited and liquid SMT balance is credited.
- HIVE/HBD/VESTS rewards: behavior matches legacy reward claim logic.

References:

- `libraries/protocol/hive_operations.cpp`
- `libraries/chain/hive_evaluator.cpp`

### 4.2 `smt_create_operation`

Purpose:

- Creates SMT using an NAI from pool, or resets an existing pre-setup SMT when fee is zero.

Authority:

- Active (`control_account`)

Validation highlights:

- Symbol must be SMT liquid NAI and match provided precision
- Precision is capped at `SMT_MAX_DECIMAL_PLACES` (currently 8)
- Creation fee must be HIVE or HBD and non-negative

Apply highlights:

- Creation path:
  - verifies symbol exists in NAI pool
  - validates/normalizes creation fee
  - burns creation fee to `HIVE_NULL_ACCOUNT`
  - creates `smt_token_object`
  - removes NAI from pool
- Reset path (`smt_creation_fee.amount == 0`):
  - only allowed for existing SMT controlled by caller
  - only in `account_elevated` phase
  - disallowed when emissions already exist
  - removes old token object then recreates

### 4.3 `smt_setup_operation`

Purpose:

- Finalizes setup parameters and creates ICO object.

Authority:

- Active (`control_account`)

Validation highlights:

- supply/cap/time invariants
- generation policy validity

Apply highlights:

- requires `account_elevated` phase
- sets token phase to `setup_completed`
- stores `max_supply`
- rejects setup when `current_supply > max_supply`
- `max_supply` is enforced by SMT supply updates in `database::adjust_supply` once set (`max_supply > 0`)
- creates `smt_ico_object` and writes generation policy/caps/window times

### 4.4 `smt_setup_emissions_operation`

Purpose:

- Adds or removes token emission schedules.

Authority:

- Active (`control_account`)

Validation highlights:

- destination constraints
- interval and endpoint constraints
- non-negative/non-zero emission requirements

Apply highlights:

- allowed only while token phase is `< setup_completed` (pre-setup)
- add:
  - schedule must be in future
  - no overlap with existing emission ranges
- remove:
  - only latest emission can be removed
  - full field equality check against latest record

### 4.5 `smt_set_setup_parameters_operation`

Purpose:

- Mutates setup parameters (`allow_voting` currently).

Authority:

- Active (`control_account`)

Validation highlights:

- non-empty `setup_parameters`

Apply highlights:

- allowed only while token phase is `< setup_completed`

### 4.6 `smt_set_runtime_parameters_operation`

Purpose:

- Sets SMT runtime params (windows, vote regen, rewards, allow downvotes).

Authority:

- Active (`control_account`)

Validation highlights:

- non-empty `runtime_parameters`
- runtime parameter-specific bounds/invariants

Apply highlights:

- allowed only while token phase is `< setup_completed`

### 4.7 `smt_contribute_operation`

Purpose:

- Records HIVE contribution for a contributor in SMT ICO.

Authority:

- Active (`contributor`)

Validation highlights:

- contribution asset must be HIVE
- amount must be positive

Apply highlights:

- requires phase in contribution window:
  - `phase >= contribution_begin_time_completed`
  - `phase < contribution_end_time_completed`
- enforces hard-cap bounds
- enforces unique `(symbol, contributor, contribution_id)`
- debits contributor HIVE balance and creates `smt_contribution_object`
- increments ICO contributed amount

### 4.8 `smt_set_token_metadata_operation`

Purpose:

- Sets token presentation fields.

Authority:

- Active (`control_account`)

Validation highlights:

- `token_name` max 32 chars
- `token_description` max 1000 chars
- `token_image_url` max 256 chars
- `token_json_metadata` must be valid JSON when non-empty

Apply highlights:

- overwrites all metadata fields on `smt_token_object`
- no phase restriction beyond token existence/control + hardfork gating

### 4.9 `smt_approve_operation`

Purpose:

- Grants or revokes delegated SMT spending.

Authority:

- Active (`owner`)

Validation highlights:

- `owner != spender`
- amount non-negative
- SMT liquid symbol required

Apply highlights:

- amount `> 0`: create or overwrite allowance
- amount `== 0`: remove allowance object (no-op if absent)

### 4.10 `smt_transfer_from_operation`

Purpose:

- Performs delegated SMT transfer using allowance.

Authority:

- Active (`spender`)

Validation highlights:

- `spender != from`
- `from != to`
- amount positive, liquid SMT symbol

Apply highlights:

- requires allowance `(from, spender, symbol)`
- requires `allowance.remaining >= amount`
- debits `from` balance and credits `to` balance
- decrements allowance, removing record when remainder is zero

### 4.11 `smt_transfer_control_operation`

Purpose:

- Transfers administrative control of an SMT to a new account.

Authority:

- Active (`control_account`)

Validation highlights:

- current `control_account` must be valid
- `symbol` must be liquid SMT symbol
- `new_control_account` must be a valid account name
- `new_control_account != control_account`

Apply highlights:

- requires caller to currently control the SMT
- requires `new_control_account` to exist
- overwrites `smt_token_object.control_account` with `new_control_account`
- subsequent SMT admin operations must be signed by the new control account

Primary references for operation validation/apply:

- `libraries/protocol/include/hive/protocol/smt_operations.hpp`
- `libraries/protocol/smt_operations.cpp`
- `libraries/chain/smt_evaluator.cpp`

## 5. SMT-Aware Existing Operations

SMT behavior is also wired into existing (non-SMT-specific) operations:

- `transfer_operation`
  - supports SMT liquid transfers because it allows any non-vesting symbol.
- `transfer_to_vesting_operation`
  - supports converting SMT liquid to SMT vesting.
- `limit_order_create_operation` / `limit_order_create2_operation`
  - support SMT:HIVE market pairs.
- `comment_options_operation` extension `allowed_vote_assets`
  - allows specifying SMT votable assets (`SMT_MAX_VOTABLE_ASSETS` limit).

Important limitation:

- `withdraw_vesting_operation` remains VESTS-only (`Amount must be VESTS`).

References:

- `libraries/protocol/hive_operations.cpp`
- `libraries/protocol/include/hive/protocol/hive_operations.hpp`
- `libraries/chain/hive_evaluator.cpp`

## 6. Database API Endpoints (SMT)

SMT endpoints are in `database_api`:

- `get_nai_pool`
- `list_smt_contributions`
- `find_smt_contributions`
- `list_smt_tokens`
- `find_smt_tokens`
- `list_smt_token_emissions`
- `find_smt_token_emissions`
- `list_smt_allowances`
- `find_smt_allowances`

### List/find contracts

General list behavior:

- `limit` must satisfy `0 < limit <= DATABASE_API_SINGLE_QUERY_LIMIT`.
- `order` must match endpoint-supported sort orders.
- `start` is order-dependent and validated at runtime.

Order/start key shapes:

- `list_smt_contributions`
  - `by_symbol_contributor`: `[symbol, contributor, contribution_id]`
  - `by_symbol_id`: `[symbol, id]`
  - `by_contributor`: `[contributor, symbol, contribution_id]`
- `list_smt_tokens`
  - `by_symbol`: `symbol`
  - `by_control_account`: `account` or `[account, symbol]`
- `list_smt_token_emissions`
  - `by_symbol_time`: `[symbol, schedule_time]`
- `list_smt_allowances`
  - `by_owner_spender`: `[owner, spender, symbol]`
  - `by_spender`: `[spender, owner, symbol]`

Find call inputs:

- `find_smt_contributions`: `symbol_contributors: [(symbol, contributor)...]`
- `find_smt_tokens`: `symbols: [symbol...]`, `ignore_precision: bool`
- `find_smt_token_emissions`: `asset_symbol`
- `find_smt_allowances`: `owner_spender_pairs: [(owner, spender)...]`, `symbol`

References:

- `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api.hpp`
- `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_args.hpp`
- `libraries/plugins/apis/database_api/include/hive/plugins/database_api/database_api_objects.hpp`
- `libraries/plugins/apis/database_api/database_api.cpp`

## 7. RC Costs (Current Implementation)

RC accounting has dedicated SMT operation entries, but current measured values are zero and SMT state-byte modeling is marked TODO.

### 7.1 Operation execution-time entries

The following execution-time constants exist in `operation_exec_info` and are used by RC operation counting:

- `claim_reward_balance2_time`
- `smt_setup_time`
- `smt_setup_emissions_time`
- `smt_set_setup_parameters_time`
- `smt_set_runtime_parameters_time`
- `smt_create_time`
- `smt_contribute_time`
- `smt_set_token_metadata_time`
- `smt_approve_time`
- `smt_transfer_from_time`
- `smt_transfer_control_time`

As currently initialized, all of the above are `0`.

### 7.2 State-byte accounting

In RC counting visitor methods for SMT operations, state byte logic is not implemented yet (`FC_TODO` markers). This means SMT-specific persistent/temporary state impacts are not explicitly priced via SMT-tailored formulas in current code.

Practical implication:

- Transactions still incur base RC components (transaction bytes/signature verification/common accounting).
- SMT-specific operation execution/state add-ons are currently under-modeled in RC formulas.

References:

- `libraries/chain/include/hive/chain/rc/resource_sizes.hpp`
- `libraries/chain/rc/resource_sizes.cpp`
- `libraries/chain/rc/resource_count.cpp`

## 8. Tests and Validation

Primary SMT unit tests:

- `tests/unit/tests/smt_operation_tests.cpp`

CTest registration:

- Included in `chain_test` selection when:
  - `ENABLE_SMT_SUPPORT=ON`
  - `BUILD_HIVE_TESTNET=ON`

Reference:

- `tests/unit/CMakeLists.txt`

Suggested run on Linux:

```bash
cmake -S . -B build-smt -DENABLE_SMT_SUPPORT=ON -DBUILD_HIVE_TESTNET=ON
cmake --build build-smt --target chain_test -j
ctest --test-dir build-smt -R 'unit/chain_test-smt_operation_tests'
```

## 9. Current Caveats / Gaps

- Token phase transitions into/out of contribution windows are required by `smt_contribute_operation`, but there is no phase-transition evaluator in the SMT operation set itself. Existing unit tests simulate phase transitions using debug updates.
- Emission schedules are validated/stored/queryable, but no in-tree emission execution path references `smt_token_emissions_object` outside setup/util/API paths.
- API object `api_smt_token_object` currently exposes copied chain objects directly (noted by existing `FIXME` comments in code).
- RC modeling for SMT-specific execution/state remains incomplete as described above.
