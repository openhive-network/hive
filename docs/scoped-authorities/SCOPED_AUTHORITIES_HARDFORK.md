# Scoped Authorities Hardfork Integration

**Status: no hardfork integration is implemented.**

## Activation strategy

Do not attach this feature to HF29: this source already defines HF29 behavior for alternate-chain builds. Mainnet's configured maximum is HF28. A future hardfork number, version, time, witness-vote policy, and testnet schedule must be assigned by maintainers. This branch must not cause automatic mainnet activation.

New operations must preserve every existing operation's binary tag. The existing variant lists 50 regular operations and then virtual operations; insertion before virtual operations shifts historic virtual tags. A reviewed compatibility mechanism is a prerequisite. Add preactivation rejection and activation-boundary tests, and keep grant state empty before activation.

## Upgrade, replay, and snapshots

All nodes must run the same implementation before witnesses schedule activation. Replay from genesis must produce identical state and blocks. Snapshot loaders must register and restore the new Chainbase index consistently. If the chosen Chainbase object layout is incompatible with older snapshots, require a documented replay or snapshot-version migration before upgrade. No snapshot compatibility result is currently established.

## Deployment checklist

- Assign hardfork slot and version through repository hardfork conventions.
- Preserve operation serialization IDs and test old transaction/block fixtures.
- Add new index registration and snapshot/replay tests.
- Build all supported configurations and run consensus, plugin, and Python integration suites.
- Test on isolated alternate-chain testnet through preactivation and activation blocks.
- Verify witness scheduling, RC accounting, rollback/reorg, and deterministic replay across two independent nodes.
- Obtain external protocol/security review before any upstream or mainnet proposal.
- Confirm mainnet activation remains solely a future witness/governance decision.
