# claim\_account\_operation, // 22

## 1. Description

A user may create a new account using a pair of operations: claim\_account\_operation and create\_claimed\_account\_operation.

After the operation claim\_account\_operation a user receives a token: Pending claimed accounts and later (using operation create\_claimed\_account\_operation) a user may create a new account.

After executing the operation claim\_account\_operation, a new account is not created.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| creator        | Account name                                                                                                                                                                                |         |
| fee            | The amount of fee for creating a new account is decided by the witnesses.<br>It may be paid in HIVE or in the Recourse Credit (RC).<br>If a user wants to pay a fee in RC, it should be set {fee= 0} |         |
| extensions     | Not currently used.                                                                                                                                                                         |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- enough HIVE or RC to pay a fee for creating an account.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- a { creator} receives a token: Pending claimed accounts.

\- HIVE or Recourse Credit balance is decreased by { fee }.

\- RC cost is paid by { account }.
