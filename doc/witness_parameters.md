# Witness Parameters

The role of a witness in the Hive blockchain network is to verify incoming transactions, produce blocks when scheduled, and partake in the Hive governance model by voting on several parameters.

These parameters control various aspects of the operation of the blockchain that are not easily defined in code at compile time. One example is the HIVE price feed that defines the conversion rate between HIVE and HBD.

Witnesses are able to use the `witness_set_properties_operation` to change witness specific properties and vote on paramters.

Unless otherwise noted, the median of the top 20 elected witnesses is used for all calculations needing the parameter.

This operation was added in Hive v0.20.0 to replace the `witness_update_operation` which was not easily extendable. While it is recommended to use `witness_set_properties_operation`, `witness_update_operation` will continue to work.

## Properties

### account_creation_fee

This is the fee in HIVE that must be paid to create an account. This field must be non-negative.

### account_subsidy_budget

The account subsidies to be added to the account subsidy per block. This is the maximum rate that accounts can be created via subsidization.
The value must be between `1` and `268435455` where `10000` is one account per block.

### account_subsidy_decay

The per block decay of the account subsidy pool. Must be between `64` and `4294967295 (2^32)` where `68719476736 (2^36)` is 100% decay per block.

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

### rc_scale

Scale of RC pools in relation to base values for 64kB blocks. If left as default `0`, it means automatic scaling with size of block (witness votes for rc_scale determined by highest multiple of 64kB fitting the size of blocks they vote for). Maximum value is `32` which is the most abundant RC (the RC costs will be roughly the same for any size of block when the same percentage of block budget is consumed, however when bigger blocks are allowed, the chance of them being less full is bigger, which results in RC costs dropping due to unused resources). When set to `1`, it means there will only be as many resources as for 64kB blocks.

Note: median value of rc_scale is always limited by median value of max_block_size, that is, even if all witnesses vote for rc_scale of `32`, but median value of max_block_size is 128kB, then the actual median rc_scale will be `2`.

### hbd_interest_rate

The annual interest rate paid to HBD holders. HBD interest is compounded on balance changes, no more than once every 30 days.

### hbd_exchange_rate

The exchange rate for HIVE/HBD to be used for printing HBD as rewards as well as HBD->HIVE conversions.
The actual price feed is the median of medians. Every round (21 blocks) the median exchange rate is pushed to a queue and the oldest is removed. The median value of the queue is used for any calculations.

### url

A witness published URL, usually to a public seed node they operate, or a post about their witness. The URL must not be longer than 2048 characters.

### new_signing_key

Sets the signing key for the witness, which is used to sign blocks.
