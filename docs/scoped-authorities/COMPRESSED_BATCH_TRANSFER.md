# Compressed Batch Transfers (experimental HF30)

## Status

This document describes the current implementation on the experimental branch. It is not a deployment proposal. The operation is gated by the same alternate-chain HF30 rule as Scoped Authorities and is rejected by production-configured evaluator builds.

## Operation and wire representation

`compressed_batch_transfer_operation` moves one HIVE or HBD asset from 1–100 source accounts to 1–100 destination accounts. The operation stores one shared asset symbol and memo. Each source and destination appears once; both vectors must be strictly sorted by account name, which also rejects duplicates. Amounts are positive signed 64-bit canonical asset units and both totals must be equal and no greater than the chain's maximum share supply. Inputs and outputs cannot overlap.

The FC binary representation uses the existing Hive serializer: a shared symbol, bounded vectors of account names and integer amounts, and one memo. This avoids repeated full `asset` objects and per-transfer memos. Account names are serialized directly; the implementation does not rely on local Chainbase IDs or a custom varint codec. An earlier signed-transaction size check recorded 3,878 bytes for 100 standard transfer operations and 1,809 bytes for an equivalent one-input batch. The later controlled four-mode RC benchmark records 5,678 versus 2,115 bytes for the 1→100 standard multi-transfer versus compressed-active rows, and 12,313 versus 10,926 bytes for 100→100; its raw CSV is the current comparison source. The different fixtures and comparison modes mean the earlier 53.35% figure should not be substituted for the later benchmark results. The follow-up signed-RPC RC benchmark covers 1-to-3, 1-to-10, 1-to-100, 3-to-2, 10-to-10, and 100-to-100 shapes with separate transactions, multiple standard transfers in one transaction, active batches, and scoped batches. The measured byte and RC tables, raw CSV, methodology, and limitations are in [COMPRESSED_BATCH_RC_BENCHMARK.md](COMPRESSED_BATCH_RC_BENCHMARK.md). No custom varint/delta codec was added because the existing serializer already provides substantial savings without a custom consensus decoder.

## Authorization

An input with `grant_id == 0` requires that source account's normal active authority. Every direct source is included in the operation's required-active set. A scoped input names its grantor in `from`, its grant ID in the input, and the common actor in the operation. Every scoped grant must independently match the actor and asset and pass status, timestamp, per-transfer, period, lifetime, balance, and recipient checks. Scoped inputs require the actor's existing active authority. The actor receives no grantor active authority.

The deterministic attribution rule processes sorted sources and sorted destinations in order: each source amount fills the remaining amount of the first destination, then subsequent destinations. A scoped grant's allowlist is checked for every destination receiving any portion attributed to that source. This is consensus-defined and prevents pooled funds from obscuring which grant funds a restricted recipient.

All grant and balance validations occur before any state change. Balance adjustments and grant-counter writes use the enclosing transaction's normal Chainbase undo session. Rejection rolls back the whole transaction; block pop restores prior balances and counters.

## Resource accounting

The operation's transaction bytes, signatures and authority checks retain the normal Hive RC accounting. The execution-time estimate additionally charges the configured transfer execution weight for every input and output. Since experimental operation tags follow historical virtual-operation tags, their RC statistics are recorded in the existing `multiop` bucket. This preserves the persistent `rc_stats_object` size and its previous checksum; cost calculation itself is per-entry. The RC vector and end-to-end RC comparison against separate transactions and a multi-transfer transaction remain unmeasured; fewer bytes alone do not establish RC savings.

## Limits and limitations

- Maximum 100 inputs and 100 outputs; all entries are bounded before evaluator work.
- One asset per operation; only HIVE and HBD.
- One shared memo.
- Duplicate source/destination entries and source/destination overlap are rejected.
- This operation does not support savings, vesting, recurrent transfers, conversions, orders or other financial operations.
- Local signed-RPC demonstrations: Alice-to-two-recipient 1-to-2 TESTS batch transaction `5bec9d138514ffecb02f2648fcd8263fed635719` was confirmed at block 63, distributing 0.500 TESTS each to Bob and merchant. Combined scoped batch transaction `3e1f6224b227388e49143ee2016e84b6207fe037` was confirmed at block 132: Bob's one signature authorized the actor role; Alice's grant 3 debited 1.000 TBD to merchant, and Bob's independent grant 1 debited 1.000 TBD to Pedro. The API showed both separate lifetime counters increase to 1.000 TBD. A follow-up signed batch attempting 1.001 TBD from Alice's grant was rejected, and RPC verified both grants remained at 1.000 TBD and all four balances unchanged. This confirms actor-authenticated multi-grant execution through RPC, including recipient attribution. Broader adversarial attribution tests now cover restricted/unrestricted source mixes, split outputs, independent grants, and reordering constraints; external audit and untested composition combinations remain required.
- A populated grant, its revoked status and spending counter survived a clean node shutdown/restart. A populated scoped-grant snapshot restore and full replay from genesis were verified for the available one-grant development chain; prior two-node agreement and broader batch shapes are recorded above. Multi-grant populated snapshot/replay coverage remains open.
- The operation is experimental consensus code and requires independent security review and a separately approved future hardfork before any production use.
