# Scoped Authorities Hardfork Integration

**Status: alternate-chain HF30 enables the experimental operations and HF31 enables grant lifecycle changes. Both are development-only; mainnet remains capped at HF28 and rejects the operations.**

## Activation strategy

Do not attach the feature to HF29: this source already defines HF29 behavior for alternate-chain builds. Mainnet's configured maximum is HF28. HF30 enables the scoped and batch operations; HF31 changes grant lifecycle behavior while keeping HF30 replay semantics intact. Both use alternate-chain configuration and a far-future fallback schedule. Mainnet builds do not add either hardfork to their active maximum; non-alternate evaluator paths contain unconditional rejection guards. A post-HF31 runtime submission test has not been performed.

Four signed operations are appended after all historic variant tags, leaving every historic numeric tag unchanged. Current full-variant tags are 93–96; the `HIVE_PROTOCOL_SKIP_VOPS` variant is expected to assign 50–53. `is_virtual_operation` returns false for the new operations. Unit tests pin existing and new tags in the ordinary build. HF30 enables the operations on alternate chains; alternate-chain-only HF31 adds grant cleanup and monotonic ID rules while preserving historical HF30 behavior. The skip-VOPS protocol-only target and executable tag check pass; full-chain builds fail at existing virtual-operation call sites, and alternate-variant serialization round-trip plus mainnet runtime rejection remain upstream validation requirements.

## Upgrade, replay, and snapshots

The Chainbase grant and grantor-metadata indexes are registered with normal database indexes. The lifecycle implementation lazily derives metadata from up to 100 retained HF30 grants, then cleans obsolete grants transactionally. Existing snapshot/replay evidence predates this new metadata type; populated HF31 snapshot restore and replay must be verified before upstream deployment. The RC statistics object layout remains unchanged.

## Deployment checklist

- Assign hardfork slot and version through repository hardfork conventions.
- Preserve operation serialization IDs and test old transaction/block fixtures.
- Add new index registration and snapshot/replay tests.
- Build all supported configurations and run consensus, plugin, and Python integration suites.
- Test on isolated alternate-chain testnet through preactivation and activation blocks.
- Verify witness scheduling, RC accounting, rollback/reorg, and deterministic replay across two independent nodes.
- Obtain external protocol/security review before any upstream or mainnet proposal.
- Confirm mainnet activation remains solely a future witness/governance decision.
