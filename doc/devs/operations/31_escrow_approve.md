# escrow\_approve\_operation, // 31

## 1. Description

The operation escrow\_approve\_operation is used to approve the escrow. The approval should be done by { to } and by the { agent }.

The escrow approval is irreversible. If { agent } or { to } haven’t approved the escrow before the { ratification\_deadline} or either of them explicitly rejected the escrow, the escrow is rejected.

If {agent} and {to} have approved the escrow, the {fee} is transferred from temporary balance to {agent} account.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------------- | ------------ |
| from             | Account\_name\_type the original ‘from’                         |         |
| to               | account\_name\_type the original 'to'                           |         |
| agent            | Account\_name\_type the original ‘agent’                        |         |
| who              | Either {to} or {agent}                                          |         |
| escrow\_id = 30; | Escrow identifier                                               |         |
| approve          | approve = true; (approve = false explicitly rejects the escrow) |         |**

## 3. Authority

Active

## 4. Operation preconditions

You need:

\- there is an escrow waiting for approval.

\- only { to } or { agent } may approve an escrow\.

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- if { agent } and { to } approved the escrow, the escrow is valid.

\- when { agent } and { to } approve the escrow, the virtual operation: escrow\_approved\_operation is generated and the {fee} is transferred from temporary balance to {agent}.

\- when either { agent } or { to } reject the escrow or the time for approval is over,  the escrow amount and {fee} are transferred back to { from } balance and the virtual operation: escrow\_rejected\_operation is generated.

\- RC cost is paid by {who}.
