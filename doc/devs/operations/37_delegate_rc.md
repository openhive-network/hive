# delegate\_rc\_operation, // 37

## 1. Description

The operation delegate\_rc\_operation sets the amount of resource credits (RC) that { from } delegates to each account listed in { delegatees }. Only RC is delegated - the vesting shares (HP), their voting power and governance rights stay with { from }.

For each delegatee:

\- if there is no delegation yet, a new one with { max\_rc } is created,

\- if there is a delegation, it is increased or decreased to { max\_rc },

\- if { max\_rc } is 0, the delegation is removed.

The operation can be used in two forms that work on the same delegations:

\- since HF 26 - embedded in custom\_json\_operation with { id } = "rc" and { required\_posting\_auths } = [ { from } ], with the operation in its JSON form, f.e. `{"type":"delegate_rc_operation","value":{"from":"alice","delegatees":["bob"],"max_rc":1000000000,"extensions":[]}}` or in legacy JSON form `["delegate_rc",{"from":"alice","delegatees":["bob"],"max_rc":1000000000,"extensions":[]}]` (more operations can be embedded as a list of operations in legacy form, f.e. `[["delegate_rc",{...}],["delegate_rc",{...}]]`),

\- since HF 29 - as a regular operation (it uses the number of reset\_account\_operation, which was never active). Before HF 29 the regular form is rejected.

The forms differ in what happens when the delegation fails. A failed regular operation fails the whole transaction, so a block that contains such a transaction is invalid. A failed embedded delegation in a new transaction also fails the transaction, but when it is already in a block, the custom\_json\_operation stays in the block and only the delegation is not effective (along with all other operations embedded in the same custom\_json\_operation). Since HF 29 the RC usage of such failed embedded operations is not counted either, so the transaction pays only for the custom\_json\_operation itself.

Account history (and other consumers of operation notifications, f.e. HAF) shows each effective embedded delegation as if it was a regular operation placed in the transaction right after its custom\_json\_operation - a non-virtual delegate\_rc\_operation with the same position in the transaction (op\_in\_trx) as the custom\_json\_operation. A non-virtual operation that repeats op\_in\_trx of the previous one is therefore not a direct part of the transaction. Embedded delegations that were not effective (all of them before HF 26, and all embedded in a custom\_json\_operation where any of embedded operations failed) are not shown. It is not tied to HF 29, so effective embedded delegations from before HF 29 are also shown (after account history is replayed).

Up to HF 28 the minimal amount of delegation and the ban on delegation during removal of excess delegations (see preconditions) were only checked for new transactions, so blocks could contain effective delegations that break them. Since HF 29 they are checked also for transactions in blocks.

The global parameters:

\- HIVE\_RC\_MAX\_ACCOUNTS\_PER\_DELEGATION\_OP, default mainnet value: 100


## 2. Parameters

| Parameter name | Description | Example |
| -------------- | ---------------------------------------------------------------------------------------------------------- | ------- |
| from           | The account delegating RC                                                                                    |         |
| delegatees     | Accounts receiving RC. Up to 100 (HIVE\_RC\_MAX\_ACCOUNTS\_PER\_DELEGATION\_OP) accounts; { from } is not allowed |         |
| max\_rc        | The amount of RC delegated to each of { delegatees }. 0 removes the delegation. Otherwise it has to be at least 1/3 of the fee for creating a new account (expressed in VESTS) |         |
| extensions     | Not used                                                                                                     |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- each of { delegatees } exists,

\- { max\_rc } is not 0 for a delegatee you don't delegate to yet, and it is different from the amount of an existing delegation,

\- enough RC that can be delegated to cover the increase of all delegations together: RC from your own vesting shares (without RC received from other accounts and without RC adjustment for account creation) that is not delegated yet, and not more than your current RC mana,

\- no excess RC delegations of { from } still being removed (when an account loses vesting shares it delegated RC from, its excess delegations are removed, and if there are too many to remove at once, the removal continues in following blocks),

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:

\- the delegatee's maximum RC and RC mana are changed by the change of the delegation,

\- the delegator's delegated RC is changed by the total change of the delegations,

\- when the delegations are increased, the delegator's RC mana is decreased by the total increase; when they are decreased or removed, the delegator's RC mana is not given back (it regenerates as usual),

\- RC cost is paid by { from }. The regular form costs less than the embedded one - it does not pay for execution of custom\_json\_operation and its binary form is smaller than JSON.
