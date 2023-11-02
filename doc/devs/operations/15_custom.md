# custom\_operation, // 15

## 1. Description

There are the following custom operations: custom\_operation, custom\_json\_operation and custom\_binary  (currently is disabled) . The operation: custom\_operation provides a generic way to add higher level protocols on top of witness consensus operations.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | --------------------------------- | ------- |
| required\_auths | flat\_set< account\_name\_type > |         |
| id = 0;         |                                  |         |
| data            | vector< char >                   |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- the custom operation is added to the blockchain.

\- RC cost is paid by the first account on {required\_auths} list.

