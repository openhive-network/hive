# API and wallet notes

The four operations and the read-only grant API are implemented in the development build. The signed examples below were confirmed on a local chain using the unified experimental HF30. Do not use mainnet keys.

## Inspect a grant

Call `database_api.get_scoped_authorities` with the grantor account:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"database_api.get_scoped_authorities","params":{"grantor":"alice"},"id":1}' \
  http://127.0.0.1:19891
```

The response includes retained grant rows and `last_issued_grant_id`. Pruned rows are absent. The high-water ID remains, so clients must choose a larger nonzero ID. If it is `4294967295`, the grantor cannot issue another ID. The API also returns configured limits, spending counters, remaining amounts, period index, expiry, recipients, and `active`/`revoked` status. It is read-only and contains no signing material.

## Create, use, revoke

Alice creates a grant for Bob with her active authority. In the HF26 JSON form, amounts use asset objects:

```json
{
  "type": "create_scoped_authority_operation",
  "value": {
    "grantor": "alice",
    "actor": "bob",
    "grant_id": 1,
    "max_per_transfer": {"amount": "5000", "precision": 3, "nai": "@@000000013"},
    "max_per_period": {"amount": "50000", "precision": 3, "nai": "@@000000013"},
    "max_lifetime": {"amount": "250000", "precision": 3, "nai": "@@000000013"},
    "period_seconds": 86400,
    "valid_from": "2026-11-01T00:00:00",
    "expires_at": "2026-11-08T00:00:00",
    "allowed_recipients": ["merchant"]
  }
}
```

The symbol `@@000000013` is HBD. For HIVE use `@@000000021`. Bob then signs a `scoped_transfer_operation` with Bob's normal active authority:

```json
{
  "type": "scoped_transfer_operation",
  "value": {
    "grantor": "alice", "actor": "bob", "to": "merchant", "grant_id": 1,
    "amount": {"amount": "1000", "precision": 3, "nai": "@@000000013"},
    "memo": "invoice"
  }
}
```

Alice revokes with `{"type":"revoke_scoped_authority_operation","value":{"grantor":"alice","grant_id":1}}`, signed by Alice's active authority. The generic wallet `sign_transaction(transaction, true)` signs and broadcasts these reflected operations. The grant does not give Bob Alice's active authority.

## Batch example

An active-authority input uses `grant_id: 0`. A scoped input names its grant ID and the operation actor. The operation requires the source accounts' active authorities for ordinary inputs and the actor's active authority for scoped inputs.

```json
{
  "type": "compressed_batch_transfer_operation",
  "value": {
    "actor": "bob",
    "symbol": {"nai": "@@000000013", "decimals": 3},
    "inputs": [
      {"from": "alice", "amount": 1000, "grant_id": 1},
      {"from": "carol", "amount": 1000, "grant_id": 1}
    ],
    "outputs": [{"to": "merchant", "amount": 2000}],
    "memo": "invoice batch"
  }
}
```

Input and output vectors must be sorted by account name. Amounts are integer smallest units. The totals must match, and a restricted input may only be attributed to recipients in its grant allowlist.

## Confirmed local requests

On the fresh consolidated chain, Alice's initial grant was confirmed in block 72 (`cbda735790cbcedfb80e863a71c97028db181b6b`). Bob's 1.000 TBD scoped transfer was confirmed in block 76 (`1016152cc3639d568f94f8380af679fb8c579655`). A scoped batch was confirmed in block 80 (`a8d8a3df7731784c2f63ef04cc0b6dc877fcfa6c`); a two-signature active batch was confirmed in block 88 (`b4c481a60e87424558e1a9a842f99d9f56819db1`). Alice revoked grant 1 in block 93 (`194ec03b1d8818c4678cb1ae2743ac9d7ecee23b`) and later created grant 3 after the old records were pruned in block 109 (`69afdd59aadef364535f5a181f9d7b8082009135`).

Over-limit, unauthorized-recipient, and expired transfers failed during pending application. They were not included in blocks. The final counters and snapshot/replay comparison are in [the testing notes](SCOPED_AUTHORITIES_TESTING.md). For local transaction examples and current block IDs, see that file.
