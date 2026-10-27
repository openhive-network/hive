# Test and local-chain results

HF30 in this branch is for alternate-chain testing only. Do not run the testnet binary against mainnet.

## Recorded automated tests

The latest recorded results are:

- Full `chain_test`: 526 cases passed; one opt-in RC benchmark case was not run. The suite reported 1,835,538 passing assertions.
- Focused Scoped Authority and Batch tests: 29 cases, 754 assertions passed.
- Configuration tests: 3 cases, 51 assertions passed.
- Full plugin suite: 102 cases, 7,134 assertions passed.
- Confirmed-block RC benchmark: 904 assertions passed.
- Python node-exit smoke tests: 2 passed in the isolated Python 3.14 environment. The complete Python integration suite was not run for this final validation.

These are the latest recorded test runs, not a rerun made with the documentation edits below. The mainnet and alternate-chain `hived` binaries had been built before the final local-chain validation. `HIVE_PROTOCOL_SKIP_VOPS` does not build the full chain in this branch or current upstream because existing code constructs virtual-operation types removed by that configuration.

Focused commands:

```sh
build-baseline/tests/unit/chain_test \
  --run_test=scoped_authority_tests --report_level=detailed
build-baseline/tests/unit/chain_test \
  --run_test=config_tests --report_level=detailed
build-baseline/tests/unit/plugin_test --report_level=detailed
```

The RC benchmark is opt-in:

```sh
mkdir -p /tmp/hf30-confirmed-benchmark-tmp
HIVE_RC_BENCHMARK_CSV="$PWD/docs/scoped-authorities/compressed_batch_rc_benchmark.csv" \
TMPDIR=/tmp/hf30-confirmed-benchmark-tmp \
build-baseline/tests/unit/chain_test \
  --run_test=scoped_authority_tests/compressed_batch_rc_benchmark \
  --report_level=detailed
```

## Signed RPC transactions

A fresh local chain used chain ID `18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e`, TESTS/TBD symbols, development accounts, and one HF30 activation at block 1. The transactions were signed with local development keys and included in produced blocks.

- Alice's grant ID 1: `cbda735790cbcedfb80e863a71c97028db181b6b`, block 72, ID `00000048b44e47533d02405506406ea633239820`.
- Bob's 1.000 TBD scoped transfer: `1016152cc3639d568f94f8380af679fb8c579655`, block 76, ID `0000004cfbdf7fa04d7dc2a6ae07398725fe2a17`.
- Scoped two-source batch from Alice and Carol: `a8d8a3df7731784c2f63ef04cc0b6dc877fcfa6c`, block 80, ID `0000005030a6e39b5834d6fd43f6737829ed814f`.
- Active-authority two-source batch: `b4c481a60e87424558e1a9a842f99d9f56819db1`, block 88, ID `00000058267d0dad89a7a1761d7bb9b44f04f43d`.
- Alice revokes grant ID 1: `194ec03b1d8818c4678cb1ae2743ac9d7ecee23b`, block 93, ID `0000005d72bd443c9b926c24ba9c452d9246a389`.
- Alice creates short-lived ID 2: `7233301ba45aca68bd988bea5b755de33e3f747b`, block 98, ID `00000062f214408eb1e96d9f0fb52aa04904ea56`.
- Alice creates ID 3 after ID 1 was revoked and ID 2 expired and was pruned: `69afdd59aadef364535f5a181f9d7b8082009135`, block 109, ID `0000006d6f228e62f880a08f639371b11ddb41a5`.
- Alice spends 1.000 TBD from ID 3: `f937eb4ed3bc319f834081302c02880a071193a8`, block 115, ID `00000073e0f171b6621695c36f6097ef254c9678`.
- Carol creates and revokes ID 2: `b40888173c510c6a8cab01e231e83749d4c5d385` in block 120, then `6f0b814d623b5e03ed0887782bf228f3530dde00` in block 121.
- Bob creates ID 2 after expired ID 1 was pruned: `a6cf2fb8365103ddfcdc022409e3bff517171bde`, block 155, ID `0000009bf0509d0afb2cf3fba10915e3beb91b39`.

The node rejected a 6.000 TBD transfer with `Transfer exceeds scoped per-transfer limit`, a transfer to an unlisted account with `Recipient is not allowed by scoped authority`, and a transfer after expiry with `Scoped authority has expired`. These failed during pending evaluation and did not enter a block. Error payloads and signed transaction responses were saved locally under `/tmp/hf30-e2e-20261019/`.

At block 170, ID `000000aae264e0ff0248dd3cab78bb2a01e0382d`, balances included Alice 96 TBD, Bob 100 TBD, Carol 98 TBD, merchant 106 TBD, and HiveDebit 100 TBD. Grant high-water IDs were Alice 3, Bob 2, Carol 2. Retained spending was Alice grant 3: 1.000 TBD, Bob grant 2: 0, Carol grant 1: 1.000 TBD. Carol's grant 2 remained revoked with zero spend. Alice's old IDs 1 and 2 and Bob's ID 1 were pruned.

## Snapshot and replay

The populated chain contained several grantors and actors, active and revoked grants, expired and pruned grants, spent allowances, and active/scoped batches. A supported snapshot was restored into an independent node. Another data directory replayed the block history from genesis.

The saved source, restored-snapshot, and replay API projections compare byte for byte. Each reports block 170 and ID `000000aae264e0ff0248dd3cab78bb2a01e0382d`. The comparison includes balances, owner/active/posting/memo authorities, grant rows, counters, status, and high-water IDs. Snapshot loading reported success at block 170; replay exited after reindexing. Machine-local projections and logs are in `/tmp/hf30-e2e-20261019/`.

## Two-node sync

Node B was stopped at block 138. Node A continued through pruning and Bob's replacement grant in block 155. B restarted with A as its only seed, caught up, and matched at block 159, ID `0000009fae5d2791a746293a50983d9c614c343e`. The API state, balances, retained grants, counters, and grantor high-water IDs matched.

## Production-configured rejection

The production-configured binary was started in a Linux network namespace with an empty route table and only loopback endpoints. The genesis `temp` account has zero-threshold authorities, so the probes needed no signatures or private keys. Each RPC broadcast reached evaluator execution and returned the expected disabled error:

- Create, revoke, scoped transfer: `Scoped authorities are disabled in mainnet builds`.
- Compressed batch: `Compressed batch transfers are disabled in mainnet builds`.

No probe was included in a block. Full responses are in `/tmp/hf30-e2e-20261019/mainnet-op-rejections.txt`.

One earlier launch attempt in this validation pass happened before namespace isolation. The compiled seed list produced outbound attempts that failed; logs show no successful peer connection. The process was stopped, and the runtime probes were repeated in the isolated namespace.

## RC benchmark

The benchmark measures signed binary bytes, signatures, resource vectors, RC, fixture submission time, process RSS, and balance-adjustment calls. It compares separate transfers, standard operations in one transaction, active batches, and scoped batches. Grant setup RC is reported separately. Read [the benchmark report](COMPRESSED_BATCH_RC_BENCHMARK.md) and raw CSV for the full results and limits.
