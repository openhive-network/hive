# Scoped Authorities Hardfork Integration

**Status: development-only HF30 gating is implemented for alternate-chain builds. Mainnet operation application remains unconditionally rejected. This is not a proposal to activate HF30 on mainnet.**

## Activation strategy

Do not attach this feature to HF29: this source already defines HF29 behavior for alternate-chain builds. Mainnet's configured maximum is HF28. The prototype adds HF30 behind alternate-chain configuration with a far-future fallback time; maintainers must review or replace that test schedule before use. Mainnet builds do not add HF30 to their active maximum and the evaluator rejects the operations.

The three operations are appended after all historic variant tags, leaving every historic numeric tag unchanged. Current tags are 93, 94, and 95; `is_virtual_operation` returns false for each. Unit tests pin the first/last historic virtual tags and all new tags. A future upstream hardfork still needs activation-boundary and preactivation-rejection tests in both alternate-chain and mainnet configurations.

## Upgrade, replay, and snapshots

The Chainbase index is registered with normal database object indexes and the feature test suite has exercised block pop rollback. A process restart and replay with non-empty grant state, snapshot restoration, and deterministic full replay have not been verified. Changing the RC stats object layout also requires compatibility review for existing state snapshots; production rollout must require a clean replay or a specifically tested migration.

## Deployment checklist

- Assign hardfork slot and version through repository hardfork conventions.
- Preserve operation serialization IDs and test old transaction/block fixtures.
- Add new index registration and snapshot/replay tests.
- Build all supported configurations and run consensus, plugin, and Python integration suites.
- Test on isolated alternate-chain testnet through preactivation and activation blocks.
- Verify witness scheduling, RC accounting, rollback/reorg, and deterministic replay across two independent nodes.
- Obtain external protocol/security review before any upstream or mainnet proposal.
- Confirm mainnet activation remains solely a future witness/governance decision.
