# create\_claimed\_account\_operation, // 23

## 1. Description

The operation create\_claimed\_account\_operation may be used by the user who has the token Pending claimed accounts (see claim\_account\_operation, // 22).

After executing the operation create\_claimed\_account\_operation, a new account is created.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------------ | ------------ |
| creator            | An account who create a new account                                                                                                                                                     |         |
| new\_account\_name | Account name<br>Valid account name may consist of many parts separated by a dot, total may have up to 16 characters, parts have to start from a letter, may be followed by numbers, or “-“. |         |
| owner              | Authority                                                                                                                                                                               |         |
| active             | authority                                                                                                                                                                               |         |
| posting            | authority                                                                                                                                                                               |         |
| memo\_key          | public\_key\_type                                                                                                                                                                       |         |
| json\_metadata     | json\_string                                                                                                                                                                            |         |
| extensions         | Not currently used.                                                                                                                                                                     |         |

## 3. Authority

Active


## 4. Operation preconditions

You need:

\- a user should have a token Pending claimed accounts (see claim\_account\_operation).

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- a new account { new\_account\_name } is created.

\- when a new account is created, the virtual operation: account\_created\_operation is generated.

\- RC cost is paid by { creator }.
