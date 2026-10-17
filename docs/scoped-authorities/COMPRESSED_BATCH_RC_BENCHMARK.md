# Compressed Batch Transfer RC benchmark

## Result scope

These are measurements from the experimental HF30 C++ chain fixture, not from a public Hive network. Every row's transactions were submitted to the fixture, included in a produced block, and checked against confirmed balances. The benchmark also verifies that each confirmed transaction's RC cost is the same value recorded at submission and that scoped lifetime counters equal their inputs. It uses HBD, matching memo bytes within each shape, identical source/output flows, and the fixture's active RC parameters and pools.

`compressed_batch_rc_benchmark.csv` is the raw result. It includes all five RC usage-vector dimensions, submitted transaction RC costs, same-pool normalized costs, scoped grant setup costs, signatures, elapsed push time, process peak RSS, and balance-adjustment call counts.

## Comparison summary

A = independent standard transfer transactions. B = one signed transaction with multiple standard transfers. C = one compressed batch with active-authority inputs. D = one scoped batch with delegated-actor authorization. Bytes and signatures are summed across A's transactions; B/C/D are one transaction each. RC percentages use the `same_pool_rc_cost` column so each mode in a shape is priced against the same captured RC pool. Confirmed `rc_charged` values closely match those normalized costs.

| Inputs → outputs | A/B/C/D bytes | A/B/C/D signatures | C byte savings vs A / B | D byte savings vs A / B | A/B/C/D confirmed RC | C RC savings vs A / B | D RC savings vs A / B |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 → 3 | 396 / 240 / 175 / 185 | 3 / 1 / 1 / 1 | 55.81% / 27.08% | 53.28% / 22.92% | 17,633,292 / 10,290,824 / 7,688,842 / 8,117,435 | 56.40% / 25.28% | 53.97% / 21.12% |
| 1 → 10 | 1,330 / 628 / 315 / 325 | 10 / 1 / 1 / 1 | 76.32% / 49.84% | 75.56% / 48.25% | 63,095,010 / 27,966,052 / 14,438,791 / 14,896,680 | 77.12% / 48.37% | 76.39% / 46.73% |
| 1 → 100 | 13,400 / 5,678 / 2,115 / 2,125 | 100 / 1 / 1 / 1 | 84.22% / 62.75% | 84.14% / 62.57% | 738,773,100 / 292,174,541 / 110,814,161 / 111,349,014 | 85.00% / 62.07% | 84.93% / 61.89% |
| 3 → 2 | 396 / 370 / 333 / 213 | 3 / 3 / 3 / 1 | 15.91% / 10.00% | 46.21% / 42.43% | 48,955,740 / 45,593,473 / 41,276,340 / 26,139,848 | 15.69% / 9.47% | 46.61% / 42.67% |
| 10 → 10 | 1,340 / 1,223 / 1,116 / 541 | 10 / 10 / 10 / 1 | 16.72% / 8.75% | 59.63% / 55.76% | 169,692,960 / 154,205,245 / 141,540,778 / 67,360,472 | 16.59% / 8.21% | 60.30% / 56.32% |
| 100 → 100 | 13,600 / 12,313 / 10,926 / 4,501 | 100 / 100 / 100 / 1 | 19.66% / 11.26% | 66.90% / 63.45% | 1,850,881,200 / 1,668,368,778 / 1,490,469,862 / 599,713,861 | 19.47% / 10.66% | 67.60% / 64.05% |

Compressed payloads retain the required signatures: active batches need one authority signature per source; scoped batches need the actor signature. The reduction is largest when one input funds many outputs and when a scoped batch replaces many source signatures. For small fan-in/fan-out, B's standard transaction can already share transaction overhead, so the extra compression yields smaller savings.

## Scoped-grant setup cost

The CSV's `grant_setup_rc` field measures confirmed creation of the grants used by D and excludes those setup operations from the D transfer-only RC cost. Setup is substantial: the recorded setup totals range from 18.84 billion RC for one grant in 1→3 to 1.843 trillion RC for 100 independent grants in 100→100. A one-use grant is therefore not RC-efficient end to end. Grant creation is a separate authorization/setup event and can be amortized only if the grant is reused; it must not be hidden when comparing a one-off payment.

The benchmark gives source accounts high development-only vesting so they can fund the transfer and grant-creation costs. This changes absolute RC regeneration versus a normal account mix but does not change the matched per-shape comparison: the same pool parameters and weights price all modes in each shape.

### Approximate RC break-even for grant setup

For each shape, `ceil(grant_setup_rc / (baseline_rc - scoped_batch_rc))` estimates how many same-shaped scoped batches would repay the grant creation RC. The result is shown in batches, not individual outputs. It assumes the same pool pricing for every use and ignores expiry, periodic allowance limits, and changes to account RC regeneration.

| Inputs → outputs | Batches vs separate transactions | Batches vs standard multi-transfer transaction |
|---|---:|---:|
| 1 → 3 | 1,981 | 8,671 |
| 1 → 10 | 391 | 1,441 |
| 1 → 100 | 30 | 104 |
| 3 → 2 | 2,431 | 2,852 |
| 10 → 10 | 1,806 | 2,128 |
| 100 → 100 | 1,473 | 1,725 |

The large 100-input setup amount includes 100 separately created grants. In a real deployment the number and duration of grants, the cadence of use, and each actor/source RC pool determine whether the setup cost can be amortized. This estimate does not assert that 100 transfers have the cost of one transfer; each batch is priced for all its inputs and outputs.

## RC accounting changes covered

Ordinary `transfer_operation` pays market-byte RC. Scoped direct transfers and compressed batches now do the same. Compressed batches charge execution weight for each input and output; every scoped input adds another transfer execution weight for reading and updating its persistent grant counter. A scoped batch selects its delegated actor as RC payer, including when active-authority inputs are mixed in. Pure active batches deterministically select the first sorted source as payer.

New operation tags remain outside the historical regular-operation range and are mapped to the existing `multiop` RC statistics bucket. This bucket is aggregate reporting only: the per-transaction RC calculation uses each operation's own resource vector and is not replaced by the bucket average. Keeping the bucket avoids changing the persistent `rc_stats_object` size/checksum. The tradeoff is that the RC stats API does not expose a distinct average for each experimental operation.

## Measurement details and limits

- `serialized_bytes` is the signed transaction's protocol binary size, including signatures and transaction overhead, not JSON size.
- A/B/C/D use identical memo contents for corresponding transfers within a shape; memo strings are unique across shapes to avoid duplicate transaction IDs.
- `same_pool_rc_cost` applies Hive's RC pricing curve to the measured resource vector using the pool/weight/regen parameters captured at the start of that shape. The `rc_charged` column is read back after block inclusion.
- `execution_us` is wall-clock time around transaction push/signature validation in this fixture, not an isolated evaluator microbenchmark. It is a single run, not a statistically controlled latency benchmark.
- `max_rss_kb` is the process high-water RSS at the row, not attributable per-transaction memory. The verified run ranged from 97,168 to 98,672 KiB (about 94.9 to 96.4 MiB).
- `balance_writes` counts evaluator balance-adjustment calls: two per ordinary transfer, or one per input and output for a batch. It is not a count of physical disk writes.
- Grant creation is confirmed with each shape's transfer rows; all transfer variants for a shape are included together in a block so their RC values share that block's pre-update pool conditions.

## Reproduction

From the repository root:

```sh
ninja -C build-baseline -j4 chain_test
mkdir -p /tmp/hf30-confirmed-benchmark-tmp
HIVE_RC_BENCHMARK_CSV="$PWD/docs/scoped-authorities/compressed_batch_rc_benchmark.csv" \
TMPDIR=/tmp/hf30-confirmed-benchmark-tmp \
build-baseline/tests/unit/chain_test \
  --run_test=scoped_authority_tests/compressed_batch_rc_benchmark \
  --report_level=detailed
```

The fixture is isolated from production networks and uses deterministic development keys. Results are specific to this build, RC parameter set, operation serialization mode, and test-chain state. They are not a promise of production RC savings or performance.
