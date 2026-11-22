# Witness Parameters

The role of a witness in the Hive blockchain network is to verify incoming transactions, produce blocks when scheduled, and partake in the Hive governance model by voting on several parameters.

These parameters control various aspects of the operation of the blockchain that are not easily defined in code at compile time. One example is the HIVE price feed that defines the conversion rate between HIVE and HBD.

Witnesses are able to use the `witness_set_properties_operation` to change witness specific properties and vote on parameters. How the values are encoded in that operation is described in [witness_set_properties_operation](./devs/operations/42_witness_set_properties.md).

The chain does not use the value of any single witness. For `account_creation_fee`, `maximum_block_size`, `hbd_interest_rate`, `account_subsidy_budget` and `account_subsidy_decay` it uses the median over the witnesses of one schedule round: the up to 21 scheduled witnesses (on mainnet the top 20 by votes among witnesses with a signing key set, plus one backup witness picked from outside the top 20 by a vote-weighted rotation), not only the top 20. Each property is sorted on its own and the value at position `n/2` of the sorted list is taken, which is the 11th of 21 values. Every 21 blocks the schedule for the next round is built and the medians are computed from the values its witnesses have at that moment; they become active when that round starts, 21 blocks later. So a change by a witness reaches the chain only if the witness is in a round built after the change, and it moves the result only if it changes which value sits in the middle. `hbd_exchange_rate` is combined differently (see below); `url` and `new_signing_key` are not combined at all.

This operation was enabled at hardfork 20 to replace the `witness_update_operation` which was not easily extendable. While it is recommended to use `witness_set_properties_operation`, `witness_update_operation` will continue to work. It always sets all of its fields at once (`url`, the signing key, `account_creation_fee`, `maximum_block_size` and `hbd_interest_rate`), so every one of them has to be given with the value the witness wants to keep.

## Properties

### account_creation_fee

This is the fee in HIVE that must be paid to create an account. It must be in HIVE, and between `0.001 HIVE` and `1000000.000 HIVE`.

### account_subsidy_budget

The account subsidies to be added to the account subsidy per block. This is the maximum rate that accounts can be created via subsidization.
The value must be between `1` and `268435455` where `10000` is one account per block.

### account_subsidy_decay

The per block decay of the account subsidy pool. Must be between `64` and `4294967295 (2^32 - 1)` where `68719476736 (2^36)` is 100% decay per block.

Below are some example values:

| Half-Life | `account_subsidy_decay` |
|:----------|------------------------:|
| 12 Hours | 3307750 |
| 1 Day | 1653890 |
| 2 Days | 826952 |
| 3 Days | 551302 |
| 4 Days | 413477 |
| 5 Days | 330782 |
| 6 Days | 275652 |
| 1 Week | 236273 |
| 2 Weeks | 118137 |
| 3 Weeks | 78757 |
| 4 Weeks | 59068 |

A more detailed explanation of resource dynamics can be found [here](./devs/2018-08-20-resource-notes.md).

### maximum_block_size

The maximum size of a single block in bytes. The value must be not less than `65536`. The value must not be more than 2MB (`2097152`).

### hbd_interest_rate

The annual interest rate paid on HBD held in savings (since hardfork 25 only savings earn interest), in hundredths of a percent: `10000` is 100%, so `1000` is 10% and `800` is 8%. The value must be between `0` and `10000`; the code default is `1000`. Interest is credited when an operation touches the savings HBD balance of an account and more than 30 days have passed since its previous interest payment. It is paid for the whole time since that payment at the rate in effect at the moment of payment.

### hbd_exchange_rate

The witness's price feed: how much HBD one HIVE is worth, given as a price with HBD as `base` and HIVE as `quote` (for example `0.250 HBD` / `1.000 HIVE`). Besides `witness_set_properties_operation`, it can be set with [feed_publish_operation](./devs/operations/07_feed_publish.md).
The actual price feed is the median of medians. On mainnet, once per hour (every 1200 blocks) the chain takes the median of the feeds of the scheduled witnesses, counting only feeds updated within the last 7 days and only when there are at least 7 of them, and pushes it to a queue of the last 84 values (3.5 days). The median of that queue is the price the chain uses (`current_median_history`), with one correction: when HBD (not counting the treasury's) would be more than 30% of the combined market cap of HIVE and HBD at that price, the price is raised to the level where it is exactly 30%. Collateralized HIVE to HBD conversions also use the lowest value in the queue and the median before that correction.
On mainnet, nodes newer than 1.28.8 also refuse to take in, relay or put into a block they produce a feed that is more than 100 times above or below the median of the queue before that correction. This is a check of the node, not a rule for blocks of other producers.

### url

A witness published URL, usually to a public seed node they operate, or a post about their witness. The URL must not be empty and must not be longer than 2048 bytes.

### new_signing_key

Sets the signing key for the witness, which is used to sign blocks. The new key applies from the witness's next block. Setting the null key (`STM1111111111111111111111111111111114T1Anm`) stops the witness from producing blocks: it is left out of schedules built after the change, and its slots in the current and the already built next round are missed. After that, `witness_set_properties_operation` can no longer be used for this witness (it must be signed with the current signing key, which nobody holds), and the witness is re-enabled with `witness_update_operation`, signed with the account's active authority (which also sets the other fields of that operation).
