# limit\_order\_cancel\_operation, // 6

## 1. Description

Cancels an order (limit\_order\_create\_operation, // 5 or limit\_order\_create2\_operation, // 21) and returns the balance to the owner.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | --------------------------------------------------------------------------------------------- | ------------ |
| owner          |                                                                                                                           |         |
| orderid = 0;   | The request\_id provided by a user during creating a limit\_order\_create\_operation or limit\_order\_create2\_operation |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:    

\- it is relevant for an operation with {fill\_or\_kill} = false.    

\- an operation  limit\_order\_create\_operation  or limit\_order\_create2\_operation with {orderid} still exists (was not fully matched nor expired).    

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:   

\- the {owner} account balance is increased by not sold {amount\_to\_sell}.    

\- RC cost is paid by {owner} account.    

\- when an existing order is canceled, the virtual operation: limit\_order\_cancelled\_operation is generated.

