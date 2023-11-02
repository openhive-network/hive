# set\_withdraw\_vesting\_route\_operation, // 20

## 1. Description

The operation set\_withdraw\_vesting\_route\_operation allows a user to decide where and how much percent of hive should be transferred to  the account { to\_account } from power down operation. A user may also decide that the Hive may be immediately converted to Hive Power.

The operation may be created in any moment of power down operation and even if there is no power down operation in progress.

The setting is valid till a user creates an operation  set\_withdraw\_vesting\_route\_operation with the same { to\_account} and with the {percent} = 0.

A user may set up 10 { to\_account } accounts. 


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| ------------------ | -------------------------------------------------------------------------------------------- | ------------ |
| from\_account  | The account the funds are coming from                                                                                                                                                                         |         |
| to\_account    | The account the funds are going to. A user may set up 10 accounts.                                                                                                                                            |         |
| percent        | The percentage of the HP shares. If the sum of the setting shares is less than 100%, the rest is transferred to the liquid balance of { from\_account }.<br>Default value: percent = 0;                           |         |
| auto\_vest     | If auto\_vest = true, then the amount of the Hive is immediately converted into HP on the { to\_account } balance.<br>If auto\_vest = false, there is no conversion from Hive into HP.<br>Default auto\_vest = false; |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- the power down operation in progress is not required.       

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the defined percent of vesting shares will be transferred to {to\_account} during every power down.

\- RC cost is paid by { from\_account}.
