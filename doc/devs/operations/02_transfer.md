# Transfer - transfer\_operation, // 2

## 1. Description

Transfer funds from {from} account to {to} account. HIVE and HBD can be transferred.

Memo for the transaction can be encrypted if message is started with '#'.

Private Memo Key must already be in the wallet for encrypted memo to work.

## 2.  Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| from           | The account the funds are coming from                                                                          |              |
| to             | The account the funds are going to                                                                             |              |
| amount         | The amount of asset to transfer from @ref from to @ref to, the allowed currency: HIVE and HBD                  | 100.000 HIVE |
| memo           | The memo is plain-text, any encryption on the memo is up to a higher level protocol, must be shorter than 2048 |              |


## 3.  Authority

Active


## 4.  Operation preconditions

You need:

\- your balance (in the currency you want to make a transfer) is equal or higher than transfer amount.

\- enough Resource Credit (RC)  to make a transfer.


## 5.  Impacted state

After the operation:

\- {from} account balance is reduced by the transferred amount.

\- {to} account balance is increased by the transferred amount.

\- if the user transfers Hive to account { hive.fund }, the virtual operation: dhf\_conversion\_operation is generated.

\- RC cost is paid by { from } account.
