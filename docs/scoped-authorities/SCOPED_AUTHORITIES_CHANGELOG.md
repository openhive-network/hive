# Change log

This records the current experimental branch. It is not a release history for Hive mainnet.

## Unified HF30 prototype

- Added grant creation, revocation, restricted transfer, and compressed batch transfer operations.
- Added persistent grant records and per-grantor high-water IDs. Revoked and expired records are pruned during later grant creation, while IDs remain permanently unavailable.
- Added consensus evaluators, HIVE/HBD limits, fixed-window counters, expiration, recipient allowlists, and transaction rollback coverage.
- Added read-only grant inspection, account-history handling, and RC resource accounting.
- Appended the new operation tags after the historical virtual-operation range and kept them out of virtual-operation classification.
- Kept all four operations behind one alternate-chain HF30 gate. There is no HF31 activation or migration.
- Added bounded multi-input/multi-output transfers with normal active authorization or scoped grants, deterministic source-to-recipient attribution, and atomic settlement.
- Added focused consensus, lifecycle, serialization, authorization, attribution, and RC tests.

## Recorded verification

The testing notes contain exact build and test counts, signed transaction IDs, block IDs, snapshot/replay comparisons, two-node synchronization, and production-configured rejection responses. The latest full test run was recorded before the most recent documentation-only edits. Do not treat the testnet prototype as production-ready.
