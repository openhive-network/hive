# Scoped Authorities API and Wallet Integration

**Status: Operations and the read-only database API are implemented in the development-only build. Signed grant, restricted transfer, revoke, batch, rejection, and expiration flows have been demonstrated on an isolated alternate-chain testnet.**

## Read API

`database_api.get_scoped_authorities` accepts `{ "grantor": "alice" }` and returns retained grants for that grantor in canonical grant ID order, plus `last_issued_grant_id`. Under HF31 clients should choose a nonzero ID greater than this value; IDs may skip, cannot wrap, and cannot be reused after pruning. If this value is `4294967295`, the grantor has exhausted its ID space. A missing or pruned grant is omitted like an unknown grant. The response also includes grantor, actor, all three configured limits, period and lifetime counters, remaining allowances, period duration/index, validity interval, recipient list, and `active`/`revoked` flags. `active` is derived from head time and the revoked flag. There is no pagination or actor-index query in this prototype. The endpoint is read-only and does not expose key material.

Smoke-tested against the modified local testnet binary using:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"database_api.get_scoped_authorities","params":{"grantor":"initminer"},"id":1}' \
  http://127.0.0.1:8091
```

The smoke test first returned an empty list for `initminer`. After signed transactions, the same call returned Alice's grant with limits, counters, allowlist, and effective status. Example query:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"database_api.get_scoped_authorities","params":{"grantor":"alice"},"id":1}' \
  http://127.0.0.1:8091
```

The demonstrated development chain uses TESTS/TBD symbols. Grant transaction `72879e657ee571a1dbf411414d7433c1b58cc798` was confirmed at block 48; Bob's 1.000 TBD transfer `b42f592e9fb9510e6f412dc20f98b5b272fee8f3` was confirmed at block 50; revoke `f90b8e4c7654e41fec644c82a6547b4f2197b3b3` was confirmed at block 55. A signed TESTS batch was confirmed at block 63 (`5bec9d138514ffecb02f2648fcd8263fed635719`). After restart the revoked state and counter remained. A short grant expired at `2026-10-17T05:13:09`, and a Bob-signed use was rejected at block 97 by consensus timestamp validation. A mixed batch with Alice and Bob each using an independent scoped HBD grant was confirmed at block 132 (`3e1f6224b227388e49143ee2016e84b6207fe037`); each counter increased by 1.000 TBD and the two intended recipients received the matching funds. An over-limit mixed batch was rejected without balance/counter changes.

The API plugin must not be required for consensus. Its response is a view of Chainbase state and may be stale until the queried block is applied.

## Wallet workflow

1. Alice signs a create operation with her normal active authority.
2. Bob signs a scoped transfer with Bob's normal active authority. Bob does not provide Alice's key.
3. The evaluator verifies the stored grant and charges counters atomically with the liquid transfer.
4. Alice signs a revoke operation with her active authority; later block-ordered transfers fail.

The new operations are not yet wrapped in dedicated CLI convenience methods, but the generic wallet `sign_transaction(transaction, true)` accepted reflected transactions with operation variants `create_scoped_authority`, `scoped_transfer`, `revoke_scoped_authority`, and `compressed_batch_transfer`. A confirmed mixed scoped batch used this operation value:

```json
{
  "actor": "bob",
  "symbol": {"nai": "@@000000013", "decimals": 3},
  "inputs": [
    {"from": "alice", "amount": 1000, "grant_id": 3},
    {"from": "bob", "amount": 1000, "grant_id": 1}
  ],
  "outputs": [
    {"to": "merchant", "amount": 1000},
    {"to": "pedro", "amount": 1000}
  ],
  "memo": "two scoped grants one batch"
}
```

The grant operation value was:

```json
{
  "grantor": "alice",
  "actor": "bob",
  "grant_id": 1,
  "max_per_transfer": "5.000 TBD",
  "max_per_period": "50.000 TBD",
  "max_lifetime": "250.000 TBD",
  "period_seconds": 86400,
  "valid_from": "2026-10-17T05:10:00",
  "expires_at": "2026-10-24T05:10:00",
  "allowed_recipients": ["merchant"]
}
```

The corresponding Bob-signed transfer was `{"grantor":"alice","actor":"bob","to":"merchant","grant_id":1,"amount":"1.000 TBD","memo":"signed local demo"}`. The generic wallet signer used the development wallet's Bob active key; Alice's active key was not present in that transaction. Use disposable development/testnet keys only. The demo wallet's startup command, chain ID, genesis spec, and witness command are recorded in `SCOPED_AUTHORITIES_TESTING.md`.
