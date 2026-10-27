# Batch transfer benchmark

These measurements come from the consolidated HF30 C++ fixture. The fixture confirmed each transaction in a block, then checked balances and RC charged. It is not a public-network benchmark.

The test compares four ways to send the same HBD amounts:

- A: separate standard transfer transactions.
- B: standard transfers in one transaction.
- C: one compressed batch using source active authorities.
- D: one compressed batch using scoped grants and the delegated actor's authority.

Each entry below lists A, B, C, D in that order. Byte counts include transaction overhead and signatures. RC savings are relative to A, then B, using the same captured pool parameters for each shape. The displayed RC totals are confirmed charges; percentages use the normalized `same_pool_rc_cost`. The exact resource vectors and raw observations are in `compressed_batch_rc_benchmark.csv`.

## Results

**1 input, 3 outputs**

- Bytes: 396, 240, 175, 185. C saves 55.81% / 27.08%; D saves 53.28% / 22.92%.
- Signatures: 3, 1, 1, 1.
- Confirmed RC: 17,633,292; 10,290,824; 7,688,842; 8,117,435.
- C saves 56.40% / 25.28% RC; D saves 53.97% / 21.12%.

**1 input, 10 outputs**

- Bytes: 1,330, 628, 315, 325. C saves 76.32% / 49.84%; D saves 75.56% / 48.25%.
- Signatures: 10, 1, 1, 1.
- Confirmed RC: 63,095,400; 27,966,102; 14,438,850; 14,896,740.
- C saves 77.12% / 48.37% RC; D saves 76.39% / 46.73%.

**1 input, 100 outputs**

- Bytes: 13,400, 5,678, 2,115, 2,125. C saves 84.22% / 62.75%; D saves 84.14% / 62.57%.
- Signatures: 100, 1, 1, 1.
- Confirmed RC: 738,792,000; 292,175,279; 110,815,281; 111,350,145.
- C saves 85.00% / 62.07% RC; D saves 84.93% / 61.89%.

**3 inputs, 2 outputs**

- Bytes: 396, 370, 333, 213. C saves 15.91% / 10.00%; D saves 46.21% / 42.43%.
- Signatures: 3, 3, 3, 1.
- Confirmed RC: 48,956,640; 45,594,342; 41,277,264; 26,140,254.
- C saves 15.69% / 9.47% RC; D saves 46.61% / 42.67%.

**10 inputs, 10 outputs**

- Bytes: 1,340, 1,223, 1,116, 541. C saves 16.72% / 8.75%; D saves 59.63% / 55.76%.
- Signatures: 10, 10, 10, 1.
- Confirmed RC: 169,698,870; 154,210,880; 141,546,906; 67,361,866.
- C saves 16.59% / 8.21% RC; D saves 60.31% / 56.32%.

**100 inputs, 100 outputs**

- Bytes: 13,600, 12,313, 10,926, 4,501. C saves 19.66% / 11.26%; D saves 66.90% / 63.45%.
- Signatures: 100, 100, 100, 1.
- Confirmed RC: 1,851,013,800; 1,668,494,261; 1,490,607,397; 599,733,022.
- C saves 19.47% / 10.66% RC; D saves 67.60% / 64.06%.

Batching shares the transaction envelope and the operation's asset symbol and memo. That explains some savings in B and C. C still needs each source's active authorization. D uses the actor's signature for the scoped inputs, but each grant is looked up, checked, and updated. The byte and RC differences therefore include batching, the operation encoding, and signature count; they are not just compression gains.

## Grant setup cost

The D transfer numbers exclude the grant creation transactions. The CSV's `grant_setup_rc` records that cost separately. It ranges from 18.84 billion RC for the single grant in the 1-to-3 shape to 1.843 trillion RC for 100 grants in the 100-to-100 shape. A one-off scoped payment does not repay this cost.

Using `ceil(grant_setup_rc / (baseline_rc - scoped_batch_rc))`, the approximate number of same-shaped batches needed to repay grant setup is:

- 1 to 3: 1,981 batches versus separate transfers; 8,671 versus standard transfers in one transaction.
- 1 to 10: 391; 1,441.
- 1 to 100: 30; 104.
- 3 to 2: 2,431; 2,852.
- 10 to 10: 1,806; 2,128.
- 100 to 100: 1,473; 1,725.

This estimate assumes each batch has the same shape and pool price. It ignores expiry, changes in available RC, and different use frequency. The 100-input setup includes 100 grants. One hundred transfers are priced as one hundred balance movements, even when submitted in one batch.

## What the fixture measures

Hive's normal RC accounting charges serialized transaction bytes. The feature adds execution weights for every input and output, plus grant-state work for scoped inputs. Grant creation is charged for the bounded retained rows scanned and for the high-water metadata record on its first creation. Scoped batches charge the actor. Active-only batches use the first sorted source as payer.

`execution_us` measures wall time around transaction submission and signature validation. It is one fixture run, not an isolated evaluator benchmark. `max_rss_kb` is process high-water memory, not memory allocated by one operation. `balance_writes` counts calls to adjust balances, not disk writes. The current run measured process RSS from 79,992 to 81,572 KiB.

The new operations use the existing `multiop` RC statistics bucket to keep the persistent RC statistics object layout unchanged. RC cost is computed from each operation's resource vector; the bucket only combines reporting. The cost model is an estimate for review, not a claim that execution work is free.

## Reproduce

```sh
ninja -C build-baseline -j4 chain_test
mkdir -p /tmp/hf30-confirmed-benchmark-tmp
HIVE_RC_BENCHMARK_CSV="$PWD/docs/scoped-authorities/compressed_batch_rc_benchmark.csv" \
TMPDIR=/tmp/hf30-confirmed-benchmark-tmp \
build-baseline/tests/unit/chain_test \
  --run_test=scoped_authority_tests/compressed_batch_rc_benchmark \
  --report_level=detailed
```

The fixture uses development keys and does not connect to a public network. Results depend on this build's RC parameters and serialization mode. The pre-consolidation CSV is retained for history; use `compressed_batch_rc_benchmark.csv` for the unified HF30 results.
