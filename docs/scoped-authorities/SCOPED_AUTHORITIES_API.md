# Scoped Authorities API and Wallet Integration

**Status: Operations and the read-only database API are implemented in the development-only build. Wallet-generated signed operation flows have not been demonstrated through a standalone testnet.**

## Read API

`database_api.get_scoped_authorities` accepts `{ "grantor": "alice" }` and returns all grants for that grantor in canonical grant ID order. The response includes grantor, actor, grant ID, all three configured limits, period and lifetime counters, remaining period and lifetime allowance, period duration/index, validity interval, recipient list, and `active`/`revoked` flags. `active` is derived from the current head time and the revoked flag. There is no pagination or actor-index query in this prototype. The endpoint is read-only and does not expose key material.

Smoke-tested against the modified local testnet binary using:

```sh
curl -sS -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"database_api.get_scoped_authorities","params":{"grantor":"initminer"},"id":1}' \
  http://127.0.0.1:8091
```

The observed response for the empty `initminer` grant set was `{"jsonrpc":"2.0","result":{"grants":[]},"id":1}`. Querying an unknown grantor returns a normal database API assertion error.

The API plugin must not be required for consensus. Its response is a view of Chainbase state and may be stale until the queried block is applied.

## Wallet workflow

1. Alice signs a create operation with her normal active authority.
2. Bob signs a scoped transfer with Bob's normal active authority. Bob does not provide Alice's key.
3. The evaluator verifies the stored grant and charges counters atomically with the liquid transfer.
4. Alice signs a revoke operation with her active authority; later block-ordered transfers fail.

Create, revoke, and transfer payloads are the reflected protocol operation schemas in `SCOPED_AUTHORITIES_SPEC.md`; serialization and operation validation are compiled into `hived`. The in-process consensus tests sign and execute these operations. An external wallet/RPC signing demo and a development-only actor account setup script remain unimplemented. Use disposable development/testnet keys only.

