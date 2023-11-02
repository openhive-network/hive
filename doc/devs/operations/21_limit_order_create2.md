# limit\_ order\_create2\_operation, // 21

## 1.  Description

This operation creates a limit order and matches it against existing open orders. It is similar to limit\_order\_create except it serializes the price rather than calculating it from other fields.

It allows to sell Hive and buy HBD or sell HBD and buy Hive.

It is a way for a user to declare they wants to sell {_amount\_to\_sell}_ Hive/HBD for at least {exchange\_rate}  per HBD/Hive.

The global parameters:

\-  HIVE\_MAX\_LIMIT\_ORDER\_EXPIRATION, default mainnet value: 28 days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------- |-------- |
| owner          |                                                |         |
| orderid = 0    | an ID assigned by owner, must be unique        |         |
| amount\_to\_sell  |                                             |         |
| fill\_or\_kill = false;   | If fill\_or\_kill = true, then the operation is executed immediately or it fails (the operation is not added to the block). If fill\_or\_kill = false, then the order is valid till { expiration}. |         |
| exchange\_rate |                                                |          |
| expiration = time\_point\_sec::maximum(); |                     |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive/HBD is equal or higher than the { amount\_to\_sell}.

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:

\- if fill\_or\_kill = true and the operation fails (the operation is not added to the block). 

\- if fill\_or\_kill = true and the operation is executed immediately, then:

- {owner} account balance is reduced by {amount\_to\_sell.

- {owner} account balance is increased by {min\_to\_receive} or more.

- the whole {amount\_to\_sell} is sold.

- RC cost is paid by {owner} account.

\- if fill\_or\_kill = false and {owner} account balance is reduced by {amount\_to\_sell} for { expiration}.

- if there is no match to any order, after { expiration} time, the {owner} account balance is increased by {amount\_to\_sell}.

- if there are some matches to some order, after {expiration} time, the {owner} account balance is increased by not sold {amount\_to\_sell}.

- {owner} account balance is increased after each match; if order is fully matched, sum of balance increases will be calculated using {exchange\_rate}.

- RC cost is paid by {owner} account.

\- when there is partially or fully match, the virtual operation fill\_order\_operation is generated.
