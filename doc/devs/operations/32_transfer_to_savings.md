# Transfer to savings - transfer\_to\_savings\_operation, // 32

## 1. Description

A user can place Hive and Hive Dollars into savings balances. Funds can be withdrawn from these balances after a three day delay.

Keeping funds on the savings balance mitigates loss from hacked and compromised accounts. The maximum amount a user can lose instantaneously is the sum of what they hold in liquid balances. Assuming an account can be recovered quickly, loss in such situations can be kept to a minimum

Additionally for keeping Hive Dollars on the savings balance, the interests are calculated.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------------------------------------- | ------------ |
| from           | The account the funds are coming from                                                             |              |
| to             | The account the funds are going to. The funds may be transferred to someone else savings balance. |              |
| amount         | The allowed currency: HIVE and HBD, amount >0                                                     | 100.000 HIVE |
| memo           | Have to be UTF8, must be shorter than 2048                                                        |              |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive or Hive Dollars is equal or higher than the operation amount.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- {from} account balance is reduced by the operation amount.

\- {to} account balance is increased by the operation amount.

\- RC cost is paid by {from} account.

\- if the balance is changed, but not more often than every 30 days, the virtual operation: interest\_operation is generated.
