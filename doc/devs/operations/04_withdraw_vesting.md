# Power down - withdraw\_vesting\_operation, // 4

## 1.  Description

This operation converts Hive Power (also called Vesting Fund Shares  or VESTS) into HIVE.

At any given point in time an account can be withdrawing from their vesting shares. A user may change the number of shares they wish to cash out at any time between 0 and their total vesting stake.

After applying this operation, vesting\_shares will be withdrawn at a rate of vesting\_shares/13 per week for 13 weeks starting one week after this operation is included in the blockchain.

This operation is not valid if a user has no vesting shares.

There can be only one withdraw\_vesting\_operation  processed at the same time.

If a user wants to stop withdraw\_vesting\_operation, they should create an operation withdraw\_vesting\_operation with amount =0. If a user creates a new withdraw\_vesting\_operation when the old one is still processed, then the old withdraw\_vesting\_operation will be canceled and a new one will be processed. 

The global parameters:

- HIVE\_VESTING\_WITHDRAW\_INTERVALS, default mainnet value: 13 weeks
- HIVE\_VESTING\_WITHDRAW\_INTERVAL\_SECONDS (HIVE\_CASHOUT\_WINDOW\_SECONDS), default mainnet value: 1 week

Before HF16, the global parameter: HIVE\_VESTING\_WITHDRAW\_INTERVALS\_PRE\_HF\_16 was used and the power down lasted 104 days.


## 2.  Parameters

| Parameter name | Description | Example |                                                                                                                              
| --------------- | ------------------------------------- | ------- |
| account         | The account the funds are coming from |         |
| vesting\_shares | Amount of VESTS (HP)                  |         |


## 3.  Authority

Active


## 4.  Operation preconditions

You need:

\- your balance in HP is equal or higher than withdraw amount.

\- enough Resource Credit (RC)  to make an operation.


## 5.  Impacted state

After the operation:

\- the HP balance will be reduced by {vesting\_shares}/13 per week for 13 weeks starting one week after this operation is included in the blockchain.

\- the Hive balance will be increased by {vesting\_shares}/13 converted into Hive per week.

\- the virtual operation: fill\_vesting\_withdraw is generated when the vesting is withdrawn.

\- RC max\_mana is reduced.

\- RC cost is paid by the {account}  (RC current\_mana is reduced).
