# update\_proposal\_votes\_operation, // 45

## 1. Description

A user may vote for proposals directly using an operation: update\_proposal\_votes\_operation, //45 or indirectly using the proxy - operation:  account\_witness\_proxy\_operation, // 13).    

A user may vote for proposals submitted by the other users. By voting for the proposal, a user may select which proposals should be funded.   

A user may vote for as many proposals as they wants, but you cannot vote twice for the same proposal.    

If a proxy is specified then all existing votes are deactivated. When the proxy is removed, the votes will be activated.   

Your vote power depends on your HP.    

If the operation account\_witness\_vote\_operation or account\_witness\_proxy\_operation or update\_proposal\_votes\_operation is not executed in HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, the votes are removed and the virtual operation: expired\_account\_notification\_operation is generated.    

The global parameters:    
\- HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, default mainnet value: 365 days

## 2. Parameters

| Parameter name | Description | Example |
| -------------- | -------------------------------------------------------------------------------------------------------------------|-------- |
| voter            | Account name                                                                                                     |         |
| proposal\_ids    | IDs of proposals to vote for/against. Before HF28 nonexisting IDs are ignored from HF28 they trigger an error.   |         |
| approve = false; | To vote for the proposal, the approve = true.<br>To remove the vote, the approve = false.                        |         |
| extensions       |                                                                                                                  |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:   

\- vote  right should not be declined (operation decline\_voting\_rights\_operation, // 36.    

\- if {approve = true}, a vote is created only when it wasn't created earlier, otherwise nothing happens.   

\- if {approve = false}, a vote is removed only when it was created earlier, otherwise nothing happens.    

\- the witness proxy can be set when there are votes for proposals.    

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After operation:   

\- the vote is added/removed.   

\- RC cost is paid by {account}.    

\- A vote is inactive when {voter} has an active proxy. When the proxy is removed, the vote will be activated.