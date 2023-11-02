# account\_create\_operation, // 9

## 1. Description

A new account may be created only by an existing account. The account that creates a new account pays a fee. The fee amount is set by the witnesses.

The account may be created in two ways:

\- using operation {account\_create\_operation},

\- using a pair of operations { claim\_account\_operation } and {create\_claimed\_account\_operation}.


## 2. Parameters
| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------- | ------------ |
| fee                | Paid by {creator}The witnesses decide the amount of the fee. Now, it is 3 HIVE.                                                                                                         |         |
| creator            | An account that creates a new account.                                                                                                                                                  |         |
| new\_account\_name | Account name<br>Valid account name may consist of many parts separated by a dot, total may have up to 16 characters, parts have to start from a letter, may be followed by numbers, or “-“. |         |
| owner              | authority                                                                                                                                                                               |         |
| active             |  authority                                                                                                                                                                              |         |
| posting            | authority                                                                                                                                                                               |         |
| memo\_key          | Not authority, public memo key.                                                                                                                                                         |         |
| json\_metadata     | json\_string                                                                                                                                                                            |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- enough HIVE to pay the fee.

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:

\- HIVE balance of {creator} is decreased by {fee}.

\- a new account { new\_account\_name}  is created.

\- when a new account is created, the virtual operation: account\_created\_operation is generated.

\- RC cost is paid by { creator }.
