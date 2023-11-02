# escrow\_release\_operation, // 29

## 1. Description

The operation is used to release the funds of the escrow. The escrow may be released by { from }, { to } or { agent } – depending on the following conditions:

- If there is no dispute and escrow has not expired, either party can release funds to the other.

- If escrow expires and there is no dispute, either party can release funds to either party.

- If there is a dispute regardless of expiration, the agent can release funds to either party following whichever agreement was in place between the parties.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------------- | ------------ |
| from            | Account\_name\_type the original ‘from’                                  |         |
| to              | account\_name\_type the original 'to'                                    |         |
| agent           | Account\_name\_type the original ‘agent’                                 |         |
| who             | the account that is attempting to release the funds.                     |         |
| receiver        | the account that should receive funds (might be {from}, might be {to})   |         |
| escrow\_id = 30 | The escrow indicator                                                     |         |
| hbd\_amount     | the amount of HBD to release                                             |         |
| hive\_amount    | the amount of HIVE to release                                            |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:   

\- a user should be {agent}, {from} or {to}.

\- enough funds on temporary escrow balance to cover {hbd\_amount} and {hive\_amount}.
 
\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- the funds are transferred from a temporary escrow balance according to the operation to { to } or { from } account.

\- RC cost is paid by { who }.
