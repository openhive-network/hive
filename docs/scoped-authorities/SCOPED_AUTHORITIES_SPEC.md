# Scoped Authorities

This describes the current experimental implementation. Scoped Authorities and compressed batches activate together under one provisional alternate-chain HF30. HF30 is not an assigned mainnet hardfork.

## Purpose and limits

A grant lets one account authorize another account to make direct liquid HIVE or HBD transfers within fixed limits. It does not add the actor to the grantor's active authority and cannot authorize legacy transfers or unrelated operations.

The implementation does not cover savings, vesting, recurrent transfers, escrow, conversions, orders, proposals, account updates, or recovery operations. It uses integer asset units and consensus block time. There is no price conversion or floating-point accounting.

## Operations

The protocol adds four signed operations:

- `create_scoped_authority_operation`
- `revoke_scoped_authority_operation`
- `scoped_transfer_operation`
- `compressed_batch_transfer_operation`

Creation and revocation require the grantor's active authority. A scoped transfer requires the actor's active authority. Existing Hive authority recursion and signature checks apply.

A grant contains the grantor, actor, grant ID, permitted asset, per-transfer limit, per-period limit, lifetime limit, period duration, valid-from time, expiry, optional recipient allowlist, spending counters, current period index, and revoked flag. Grants are immutable. To change a grant, revoke it and create a new one with a greater ID.

## Grant IDs and record cleanup

Grant IDs are nonzero `uint32_t` values and must increase for each grantor. A persistent high-water record remains after grant records are deleted. IDs may skip, but cannot wrap or be reused. `UINT32_MAX` can be issued once; that grantor cannot issue another ID afterward.

A grantor may have up to 100 retained grant records. On grant creation, the evaluator examines that bounded set and removes records already revoked or expired at the current head-block time. Active grants are never pruned. There is no background cleanup. Pruned grants no longer appear in the inspection API, but their IDs remain covered by the high-water record and operation history remains in the blockchain.

## Spending and time

The valid interval is `valid_from <= head_block_time < expires_at`. A transfer at the exact expiry timestamp fails. Periods are fixed windows anchored at `valid_from`:

```text
period_index = (head_block_time - valid_from) / period_seconds
```

When the index changes, period spending resets. Lifetime spending never resets. For each transfer, consensus checks the asset, actor, recipient, time, revocation status, liquid balance, per-transfer limit, period remaining amount, and lifetime remaining amount before changing state.

If validation fails, the transaction undo session restores balances, counters, and pruning changes. Revocation prevents future use but preserves spending counters until ordinary deterministic pruning removes the record.

## Batches

A batch can combine active-authority inputs and scoped inputs. It uses one asset, sorted unique sources and destinations, positive integer amounts, and exact input/output conservation. The actor's authority is required for scoped inputs; each ordinary input requires its source authority.

Outputs are attributed to inputs by walking sorted sources and sorted destinations. Each source consumes destination amounts in order. A scoped grant's allowlist must include every destination that receives any portion attributed to that source. This rule is part of consensus and is covered by recipient-attribution tests.

## Example

Alice creates a grant for Bob with a 5.000 HBD per-transfer limit, 50.000 HBD per day, 250.000 HBD lifetime limit, and merchant allowlist. Alice signs creation with her active authority. Bob signs a scoped transfer with Bob's active authority. The chain debits Alice only after checking Bob, the grant, the merchant, the balance, and all spending limits.

The full JSON forms and RPC examples are in [the API notes](SCOPED_AUTHORITIES_API.md). Implementation evidence and limitations are in [the testing notes](SCOPED_AUTHORITIES_TESTING.md).
