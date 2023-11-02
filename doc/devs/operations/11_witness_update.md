# witness\_update\_operation, // 11

## 1. Description

The operation witness\_update\_operation may be used to become a new witness or to update witness properties.

There are two operations that allow to update witness properties witness\_update\_operation and witness\_set\_properties\_operation. In order to update witness properties it is recommended to use witness\_set\_properties\_operation.
 

If a user wants to become a witness, the operation witness\_update\_operation should be created.

If the witness doesn’t want to be a witness any more, the operation witness\_update\_operation with empty { block\_signing\_key} should be created.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| Parameter name      | Description                                                                                                               | Example |
| owner               | The witness who wants to update properties or a user who wants to become a witness                                        |         |
| url                 | url to information about witness                                                                                          |         |
| block\_signing\_key | Public block signing key                                                                                                  |         |
| props               | legacy\_chain\_properties                                                                                                 |         |
| fee                 | The asset is validated (the format should be correct and should be expressed in Hive), but the fee is currently ignored.  |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- if the operation was used to become a witness, the balance is decreased by {fee}.

\- a witness is created or the properties are updated.

\- RC cost is paid by { owner }.
