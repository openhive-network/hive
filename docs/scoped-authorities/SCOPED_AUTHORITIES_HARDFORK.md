# Experimental HF30

HF30 is a provisional development identifier used only by alternate-chain builds. It is not an assigned Hive mainnet hardfork number. Hive maintainers must choose the production activation point and identifier.

## Activation

One activation enables all four operations and lifecycle rules together:

- `create_scoped_authority_operation`
- `revoke_scoped_authority_operation`
- `scoped_transfer_operation`
- `compressed_batch_transfer_operation`

There is no HF31 path, intermediate migration, or dependency on a second hardfork. Production-configured evaluator builds reject all four operations. The alternate-chain test schedule used for the local demonstration activated HF30 at block 1.

## State and compatibility

Grant records and per-grantor high-water IDs are Chainbase objects. Metadata is created with a grant. There is no activation-time migration. Revoked and expired grants are pruned on later grant creation; the high-water ID remains so deleted IDs cannot return.

Nodes that run this protocol must use the modified binary to validate blocks containing these operations and maintain grant state. The local populated-chain test restored a snapshot and replayed from genesis with matching state. Those results do not establish compatibility with a future mainnet snapshot format or older node binaries.

The four signed operation tags are appended after historical virtual-operation tags so existing tags do not move. The operations remain regular signed operations. The new tags use the existing RC `multiop` statistics bucket to preserve the persistent RC stats layout. These choices need review across every supported build configuration before an upstream hardfork is proposed.

## Before any production proposal

Hive maintainers and witnesses would need to agree on the assigned hardfork, activation schedule, upgrade window, replay requirements, snapshot compatibility, and any migration policy. The implementation also needs independent security and RC economics review. HF30 must not be enabled on mainnet as it appears in this experimental branch.
