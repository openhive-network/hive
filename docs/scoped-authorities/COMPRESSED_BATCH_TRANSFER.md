# Compressed batch transfers

`compressed_batch_transfer_operation` is an experimental HF30 operation for one HIVE or HBD asset. It debits 1 to 100 accounts and credits 1 to 100 accounts in one atomic operation. HF30 is a development placeholder, not a mainnet activation.

## Operation data

The operation contains one asset symbol, one memo, a vector of inputs, and a vector of outputs. Each input has a source account, an integer amount in the asset's smallest unit, and a grant ID. Grant ID 0 means the source uses its normal active authority. A nonzero ID selects a scoped grant belonging to that source. Each output has a destination and an integer amount.

Both vectors must be strictly sorted by account name, which also rejects duplicates. Amounts must be positive. Sources and destinations cannot overlap. The input total must equal the output total and fit the protocol's share supply bound.

The binary format uses Hive's existing FC serializer. Account names are serialized directly. The symbol and memo are shared by the operation instead of repeated for every transfer. There is no custom varint, delta codec, or compression layer.

Example payload for two scoped sources paying one merchant:

```json
{
  "actor": "bob",
  "symbol": {"nai": "@@000000013", "decimals": 3},
  "inputs": [
    {"from": "alice", "amount": 1000, "grant_id": 1},
    {"from": "carol", "amount": 1000, "grant_id": 1}
  ],
  "outputs": [{"to": "merchant", "amount": 2000}],
  "memo": "invoice batch"
}
```

## Authorization and settlement

Every ordinary input requires that source's active authority. A scoped input requires the operation actor's active authority and a matching unexpired, unrevoked grant for the source and asset. If one grant is referenced by multiple inputs, validation aggregates its spend before checking limits.

For recipient checks, the evaluator attributes amounts deterministically. It walks sources in sorted order and consumes destinations in sorted order. If an input pays more than the remaining amount of one destination, the rest goes to the next destination. A restricted grant must allow every destination receiving any part of its input.

The evaluator validates authorities, grants, recipients, balances, limits, and conservation before applying any balance or counter change. Normal transaction undo rolls the operation back on failure and restores its state on block pop.

## RC and limits

RC accounts for transaction bytes and signatures as usual. The feature also adds execution cost per input and output, plus grant-state work for each scoped input. A scoped batch charges its actor. An active-only batch charges its first sorted source. Detailed confirmed-block measurements are in [the RC benchmark](COMPRESSED_BATCH_RC_BENCHMARK.md).

The operation is limited to 100 inputs and 100 outputs, HIVE or HBD, and one shared memo. It cannot move savings, vesting, recurrent transfers, conversions, or market assets. Tests cover conservation, authorization, allowlist attribution, overlap, multiple grants, rollback, and size/RC accounting. The feature still needs independent security and economics review.
