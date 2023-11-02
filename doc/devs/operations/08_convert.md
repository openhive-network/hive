# Convert - convert\_operation, // 8

## 1. Description

This operation instructs the blockchain to start a conversion of HBD to Hive. The funds are deposited after 3.5 days.   

The global parameters:

- HIVE\_CONVERSION\_DELAY, default mainnet value: 3.5 days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------- | ------- |
| owner          | account\_name\_type                                         |         |
| requestid = 0; | The number is given by a user. Should be unique for a user. |         |
| amount         | Amount > 0, have to be in HBD                               |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive Dollars is equal or higher than the operation amount.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- immediately after the operation, {owner} account balance in HBD is reduced by the operation amount.

\- 3.5 days after the operation, the  {owner} account balance in Hive is increased by the operation amount exchanged on the median feed price from that moment. Median feed price is the median of the median prices provided by the witnesses during those 3.5 days.

\- when the balance is increased, the virtual operation: fill\_convert\_request\_operation is generated. 

\- RC cost is paid by account {owner}.
