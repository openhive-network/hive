# delegate\_vesting\_shares\_operation, // 40

## 1. Description

The operation delegate\_vesting\_shares\_operation allows to delegate an amount of  { vesting\_shares } to an { delegatee } account. The { vesting\_shares} are still owned by { delegator }, but the voting rights and resource credit are transferred.

A user may not delegate:

-  the vesting shares that are already delegated.
-  the delegated vesting shares to him (a user does not own them).
-  the vesting shares in the Power down process.
-  the already used voting shares for voting or downvoting.

In order to remove the vesting shares delegation, the operation delegate\_vesting\_shares\_operation should be created with {vesting\_shares = 0}. When a delegation is removed, the delegated vesting shares are frozen for five days (HIVE\_DELEGATION\_RETURN\_PERIOD\_HF20) to prevent voting twice.

The global parameters:

- HIVE\_DELEGATION\_RETURN\_PERIOD\_HF20, default mainnet value: 5 days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------------------------------------------------------------------- | ------- |
| delegator       | The account delegating vesting shares                                                                     |         |
| delegatee       | The account receiving vesting shares                                                                      |         |
| vesting\_shares | The amount of vesting shares to be delegated. Minimal amount = 1/3 of the fee for creating a new account. |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in HP is equal or higher than { vesting\_shares }.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the delegatee’s vesting shares (HP balance) is increased by { vesting\_shares }.

\- the delegatee’s RC is increased by { vesting\_shares }.

\- the delegator’s vesting shares (HP balance) is decreased by { vesting\_shares }.

\- the delegator’s RC is decreased by { vesting\_shares }.

\- if the delegator removes the vesting shares delegation, the vesting shares are frozen for one week.

\- when the delegator receives the delegated vesting shares back, the virtual operation: return\_vesting\_delegation\_operation is generated.  

\- RC cost is paid by { delegator }.
