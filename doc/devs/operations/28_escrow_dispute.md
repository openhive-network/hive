# escrow\_dispute\_operation, // 28

## 1. Description

The operation  escrow\_dispute\_operation is used to raise the dispute. It may be used by { from } or { to } accounts.If there is a dispute, only {agent} may release the funds.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------- | ------------ |
| from             | Account\_name\_type the original ‘from’             |         |
| to               | account\_name\_type the original 'to'               |         |
| agent            | Account\_name\_type the original ‘agent’            |         |
| who              | account\_name\_type, Either {from} or {to} account. |         |
| escrow\_id = 30; | The escrow identifier                               |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:

\- a user should be {from} or {to}.
 
\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- neither {from} nor {to} may release the funds (operation: escrow\_release\_operation).

\- RC cost is paid by {who}.
