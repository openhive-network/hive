# account\_witness\_vote\_operation, // 12

## 1. Description

A user may vote for a witness directly using an operation: account\_witness\_vote\_operation or indirectly using the proxy - operation:  account\_witness\_proxy\_operation, // 13.    
All accounts with a Hive Power (also called Vesting Fund Shares  or VESTS) can vote for up to 30 witnesses, but you cannot vote twice for the same witnesses.     
If a proxy is specified then all existing votes are removed.Your vote power depends on your HP.     
If the operation account\_witness\_vote\_operation or account\_witness\_proxy\_operation or update\_proposal\_votes\_operation is not executed in a HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, the votes are removed and the virtual operation: expired\_account\_notification\_operation is generated.    

The global parameters:    
\- HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, default mainnet value: 365 days

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------------------------- | ------- |
| account         |                                                                                      |         |
| witness         | Witness account                                                                      |         |
| approve = true; | To vote for the witness, the approve = true.To remove the vote, the approve = false. |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:    

\- vote right should not be declined (operation decline\_voting\_rights\_operation, // 36).   

\- the witness proxy cannot be set.    

\- if {approve = true}, the {witness} cannot be already on the list of witnesses the user already voted for. If {approve = false}, the witness has to be on the list of witnesses the user already voted for.    

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:    

\- the vote is added/removed.    

\- RC cost is paid by {account}.
