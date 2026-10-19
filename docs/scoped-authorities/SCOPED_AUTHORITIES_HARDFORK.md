# Unified Experimental HF30 Integration

**Status:** HF30 is a provisional development-only identifier on alternate-chain builds. It is not an assigned Hive mainnet hardfork number. Scoped Authorities, Compressed Batch Transfers, and grant lifecycle rules activate together at this one hardfork.

## Activation

Alternate-chain builds expose hardfork slot 30 after the existing alternate-chain HF29 behavior. Their development schedule defaults to a far-future time unless a local chain specification schedules HF30. Mainnet builds retain HF28 as their configured maximum and compile evaluator guards that reject all four experimental operations. No second experimental activation or intermediate lifecycle state exists in this branch.

All four signed operations become available together after HF30: `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, `scoped_transfer_operation`, and `compressed_batch_transfer_operation`. HF30 has no state migration. Grantor high-water metadata is created atomically with that grantor's first grant, and all subsequent grant creation uses monotonic IDs and deterministic pruning.

The four operation tags remain appended after the historical variant tags; no existing operation is renumbered. The full operation variant uses tags 93–96 in the current build. The `HIVE_PROTOCOL_SKIP_VOPS` protocol variant has its own append layout. Full-chain compilation with that macro fails at an existing virtual-operation construction omitted from the variant in both this branch and upstream develop. Compatibility for each supported build variant must be reviewed before upstream adoption.

## State and replay

The Chainbase grant and grantor-metadata indexes are registered through the normal database index mechanism. HF30 activation itself changes no grant rows. The first grant creation stores the highest issued ID; later creations prune only revoked or expired rows, with the existing 100-record bound limiting work. The metadata remains after all grant rows are pruned, so identifiers cannot be reused. Failed transactions and block pops restore both metadata and grant records through the normal undo session.

This branch consolidates previously experimental two-slot development histories. Those chains and snapshots are intentionally not replay-compatible with the unified schedule. Preserve them on the prior branch for reference; use fresh data directories and a fresh genesis specification for consolidated testing. No existing data directory is automatically rewritten or removed.

## Verification and deployment requirements

- Schedule only HF30 in isolated alternate-chain specs and test transactions immediately before and after activation.
- Verify the four operations are rejected in a production-configured local harness.
- Preserve historical operation tags and serialization fixtures.
- Run consensus, plugin, configuration, RC, signed-RPC, snapshot, full replay, and two-node synchronization tests.
- Review snapshot/state layout and replay requirements with Hive maintainers.
- Obtain an independent protocol/security audit and normal witness/governance approval before any production proposal.
- Replace the provisional identifier and schedule through upstream hardfork conventions if maintainers choose to pursue deployment.
