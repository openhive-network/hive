# Scoped Authorities API and Wallet Integration

**Status: API design only. No endpoint or operation is implemented.**

## Read API

Expose a read-only database API query by grantor and grant ID, and a bounded paginated query by grantor or actor. Return the ID, grantor, actor, allowed symbol, configured limits, lifetime and current-period usage, remaining lifetime/period allowance, allowlist, validity interval, effective status (`pending`, `active`, `expired`, or `revoked`), and revocation flag. Remaining values are derived with saturating subtraction and status uses current chain head time. Never expose key material.

The API plugin must not be required for consensus. Its response is a view of Chainbase state and may be stale until the queried block is applied.

## Wallet workflow

1. Alice signs a create operation with her normal active authority.
2. Bob signs a scoped transfer with Bob's normal active authority. Bob does not provide Alice's key.
3. The evaluator verifies the stored grant and charges counters atomically with the liquid transfer.
4. Alice signs a revoke operation with her active authority; later block-ordered transfers fail.

The JSON examples in `SCOPED_AUTHORITIES_SPEC.md` are proposed shapes and must not be treated as accepted wire/API payloads until serializer tests exist. No demo script is implemented yet. Use disposable development/testnet keys only.

