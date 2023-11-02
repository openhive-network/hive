# Cancel transfer from savings - cancel\_transfer\_from\_savings\_operation, // 34

## 1. Description

Funds withdrawals from the savings can be canceled at any time before it is executed.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------------------------------ | ------- |
| from             | account\_name\_type                                                                      |         |
| request\_id = 0; | The request\_id provided by a user during creating a transfer\_from\_savings\_operation. |         |

 
## 3. Authority

Active


## 4. Operation preconditions

You need:

\- an operation  transfer\_from\_savings\_operation exists and is not executed.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- {from} account savings balance is increased by the operation amount.

\- RC cost is paid by {from} account.
