# Limit order create - limit_order_create_operation, // 5

## 1. Description

This operation creates a limit order and matches it against existing open orders. It allows to sell Hive and buy HBD or sell HBD and buy Hive.

It is a way for a user to declare they want to sell {amount_to_sell} Hive/HBD for at least {min_to_receive} HBD/Hive.

The user may be a taker (if a user creates an order and the order matches some order(s)) or a maker (if a user creates an order and the order doesn’t match and the order is waiting for a match on the market). If there is a partial match, a user may be a taker and maker for the same order. 

If a taker creates an order for all orders on the market the order(s) that are the best deal for the taker are matched. If there are two orders with the same price, the older one is matched.

The operation is used by the markets see: https://wallet.hive.blog/market

The global parameters:

\- HIVE_MAX_LIMIT_ORDER_EXPIRATION, default mainnet value: 28 days

## 2. Parameters

| Parameter name | Description | Example |
|----------------|-------------|---------|
| owner |  |   |
| orderid = 0 | an ID assigned by owner, must be unique |   |
| amount_to_sell |  |  |
| min_to_receive |  |   |
| fill_or_kill = false; | If fill_or_kill = true, then the operation is executed immediately or it fails (the operation is not added to the block). <br>If fill_or_kill = false, then the order is valid till { expiration}. |   |
| expiration = time_point_sec::maximum(); |  |   |

## 3. Authority

Active

## 4. Operation preconditions

You need:

\- your balance in Hive/HBD is equal or higher than the {amount_to_sell}.

\- enough Resource Credit (RC)  to make an operation.

## 5. Impacted state

After the operation:

\- if fill_or_kill = true and the operation fails (the operation is not added to the block). 

\- if fill_or_kill = true and the operation is executed immediately, then:

- {owner} account balance is reduced by {amount_to_sell}.

- {owner} account balance is increased by {min_to_receive} or more.

- the whole {amount_to_sell} is sold, 

- RC cost is paid by {owner} account.

\- if fill_or_kill = false and {owner} account balance is reduced by {amount_to_sell} for { expiration}.

- if there is no match to any order, after { expiration} time, the {owner} account balance is increased by {amount_to_sell}.

- if there are some matches to some order, after {expiration} time, the {owner} account balance is increased by not sold {amount_to_sell}.

- {owner} account balance is increased after each match; if order is fully matched, the sum of balance increases will be  {min_to_receive} or more.

- RC cost is paid by {owner} account.

\- when there is partially or fully match, the virtual operation fill_order_operation is generated.
