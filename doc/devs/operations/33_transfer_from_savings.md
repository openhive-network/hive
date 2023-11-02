# Transfer form savings - transfer\_from\_savings\_operation, // 33

## 1. Description

Funds withdrawals from the savings to an account take three days.

The global parameters: 

\- HIVE\_SAVINGS\_WITHDRAW\_TIME, default mainnet value: 3 days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| ---------------- | ----------------------------------------------------------- | ------- |
| from             | account\_name\_type                                         |         |
| request\_id = 0; | The number is given by a user. Should be unique for a user. |         |
| to               | account\_name\_type                                         |         |
| amount           | The allowed currency: HIVE and HBD, amount >0               |         |
| memo             | Have to be UTF8,  must be shorter than 2048                 |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive or Hive Dollars on a savings account is equal or higher than the operation amount.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- immediately after the operation, {from} account balance is reduced by the operation amount.

\- three days after the operation, {to} account balance is increased by the operation amount.

\- when the balance is increased, then the virtual operation: fill\_transfer\_from\_savings is generated. 

\- RC cost is paid by {from} account.
