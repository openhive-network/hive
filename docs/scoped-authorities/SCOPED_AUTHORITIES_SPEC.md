# Scoped Authorities Protocol Specification

**Status: a development-only consensus prototype is implemented. The schemas and limits below describe the current source; broader testnet, snapshot/replay, and external security validation remain incomplete.**

## Goals and non-goals

Scoped Authorities let an account delegate a narrowly defined ability to make direct liquid HIVE or HBD transfers. Consensus state, signed transactions, block time, and normal balance checks are the sole enforcement inputs. Existing owner, active, and posting authorities remain unchanged. A scoped grant does not authorize legacy `transfer_operation` or any operation other than the dedicated scoped transfer.

Version one excludes savings, recurrent transfers, escrow, vesting, conversions, orders, proposals, account/recovery changes, and every other operation. It uses no price conversion and no floating point.

## Protocol design

Three regular operations are implemented: `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, and `scoped_transfer_operation`. No update operation is provided. Changing limits requires revoking the old grant and creating a new grant with a distinct grant identifier; each grant has independent counters.

Creation and revocation require the grantor's **active** authority. This matches the authority used for liquid transfers and allows recovery through normal owner-to-active authority upgrade semantics. Owner authorization can satisfy an active requirement under current transaction verification rules; the owner key is not separately required. Scoped transfer requires the actor's normal active account authority, never the grantor's authority. It has no `account_auths` mechanism and is not added to authority recursion or signature verification shortcuts.

The grant record contains the grantor and actor account names, caller-selected `uint32_t` grant ID, asset-specific per-transfer/lifetime/period amounts, period seconds, valid-from and expiration timestamps, sorted recipient account names (empty means unrestricted), lifetime spent, current period index and period spent, and a revoked bit. A unique index on `(grantor, grant_id)` and lookup index on `(actor, grantor, grant_id)` support deterministic access. IDs cannot be reused while the tombstone exists. Creation is bounded at 100 grants per grantor; recipient lists are bounded at 32 accounts.

## Operation schemas (JSON shape)

```json
{"type":"create_scoped_authority_operation","value":{
  "grantor":"alice","actor":"bob","grant_id":1,
  "max_per_transfer":{"amount":"5000","precision":3,"nai":"@@000000013"},
  "max_lifetime":{"amount":"250000","precision":3,"nai":"@@000000013"},
  "max_per_period":{"amount":"50000","precision":3,"nai":"@@000000013"},
  "period_seconds":86400,"valid_from":"2026-11-01T00:00:00",
  "expires_at":"2026-11-08T00:00:00","allowed_recipients":["merchant1"]}}
```

```json
{"type":"scoped_transfer_operation","value":{
  "grantor":"alice","actor":"bob","grant_id":1,"to":"merchant1",
  "amount":{"amount":"5000","precision":3,"nai":"@@000000013"},"memo":"invoice"}}
```

Revocation identifies `(grantor, grant_id)` and requires grantor active authority. Its static-variant JSON shape is `{"type":"revoke_scoped_authority_operation","value":{"grantor":"alice","grant_id":1}}`. These JSON shapes reflect protocol field names; wallet/RPC signing interoperability remains to be demonstrated.

## Spending and timestamp semantics

All amounts use positive canonical `asset` integer units. For every successful scoped transfer, consensus validates the asset, destination, grant identity, actor signature, time interval, grant status, balance, and each spending ceiling before changing state. Lifetime and period additions use checked arithmetic; a failed check throws and the transaction's normal undo session rolls back all balance and counter writes.

The valid interval is half-open: `valid_from <= head_block_time < expires_at`. The fixed period index is `(head_block_time - valid_from) / period_seconds`; the period counter logically starts at zero when the index changes. Time is consensus block time. Lifetime spent never resets. Multiple operations in one transaction, multiple transactions in one block, and multiple grants use their own persistent counters in operation order.

Exact-expiration transfers fail. Expired and revoked objects remain stored as tombstones so IDs cannot be reused and API status is inspectable; per-account grant caps bound persistent state. Tombstone pruning is not part of v1.

## Hardfork and compatibility

This checkout contains HF28 as its mainnet maximum (`HIVE_NUM_HARDFORKS` is 28 outside alternate-chain builds); HF29 is defined for alternate-chain builds. The prototype gates operations behind development HF30 for alternate-chain builds and rejects them unconditionally in mainnet builds. HF30 is not activated on mainnet by this branch.

Existing regular operation tags are 0–49 and existing virtual-operation tags are 50–92. The new regular operations are appended with tags 93–95, and tests check that they are non-virtual and that existing transfer, recurrent-transfer, and virtual-operation tags remain fixed. A complete binary golden/replay compatibility suite is still needed. Before HF30 all scoped operations are rejected. The new index is registered normally; populated state migration and snapshot compatibility have not been demonstrated.

## Security invariants

1. Only grantor active-authorized creation/revocation changes grant policy.
2. Actor active authority authorizes only the dedicated operation and cannot confer authority over the grantor.
3. A transfer consumes exactly one matching grant's counters.
4. Every limit and destination check precedes transfer success; state is transactionally undoable.
5. Existing transfer and other operations remain governed by their existing authority paths.
6. No client clock, oracle, background worker, or off-chain state affects acceptance.
7. Expiration, revocation, and usage are based on deterministic state and block order.
