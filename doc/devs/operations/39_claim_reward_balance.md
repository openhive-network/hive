# claim\_reward\_balance\_operation, // 39

## 1. Description

An operation claim\_reward\_balance\_operation is used to transfer previously collected author and/or curation rewards from sub balance for the reward to regular balances. Rewards expressed in Hive and HBD are transferred to liquid balances, rewards in HP increase vesting balance. When claimed, HP rewards are immediately active towards governance voting power (compare with transfer\_to\_vesting\_operation)


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------- | ------- |
| account        | Account name                                                  |         |
| reward\_hive   | The amount of Hive reward to be transferred to liquid balance |         |
| reward\_hbd    | The amount of HBD reward to be transferred to liquid balance  |         |
| reward\_vests  | The amount of HP reward to be transferred to vesting balance  |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- the reward (author or curator)  should be calculated.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the reward is transferred to the user balance – the balance is increased by the transferred amount ( in HIVE, HBD or HP).

\- RC cost is paid by { account }.
