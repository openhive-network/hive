# escrow\_transfer\_operation, // 27

## 1. Description

The escrow allows the account { from } to send money to an account { to } only if the agreed terms will be fulfilled. In case of dispute { agent } may divide the funds between { from } and { to }. The escrow lasts up to { escrow\_expiration }.    

When the escrow is created, the funds are transferred {from} to a temporary account.  The funds are on the temporary balance, till the operation escrow\_release\_operation is created.

To create an valid escrow:    

1.  Sender { from } creates the escrow using the operation: escrow\_transfer\_operation indicated  { to } and { agent }.   

2.  The { agent } and { to } have up to { ratification\_deadline } for approving the escrow using operation: escrow\_approve\_operation.

If there is a dispute, the operation: escrow\_dispute\_operation should be used.

In case of the escrow releases, the operation: escrow\_release\_operation should be used.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | --------------------------------------- | ------------ |
| from                                     | account\_name\_type                                                                                                                                                               |         |
| to                                       | account\_name\_type                                                                                                                                                               |         |
| agent                                    | account\_name\_type                                                                                                                                                               |         |
| escrow\_id = 30;                         | It is defined by the sender.It should be unique for { from }.                                                                                                                     |         |
| hbd\_amount = asset( 0, HBD\_SYMBOL );   | Escrow amount.                                                                                                                                                                    |         |
| hive\_amount = asset( 0, HIVE\_SYMBOL ); | Escrow amount.                                                                                                                                                                    |         |
| fee                                      | The fee amount depends on the agent.The fee is paid to the agent when approved.                                                                                                   |         |
| ratification\_deadline                   | Time for approval for { to } and { agent }.<br>If the escrow is not approved till { ratification\_deadline }, it will be rejected and all funds returned to { from }.time\_point\_sec |         |
| escrow\_expiration                       | See description of escrow\_release\_operation.time\_point\_sec                                                                                                                    |         |
| json\_meta                               |                                                                                                                                                                                   |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:

\- enough Hive or HBD to make a transfer.

\- enough Hive or HBD for a fee for an {agent}.
 
\- enough Resource Credit (RC)  to make an operation.

## 5. Impacted state

After the operation:

\- the escrow amount is frozen, it is transferred to a temporary account after the operation escrow\_approve\_operation.

\- the Hive or HBD balance of {from} is decreased by { fee } for an agent.

\- when either { agent } or { to } reject the escrow or the time for approval is over, the escrow amount and {fee} are  transferred back to { from } balance and the virtual operation: escrow\_rejected\_operation is generated.

\- RC cost is paid by { from }.
