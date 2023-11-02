# account\_witness\_proxy\_operation, // 13

## 1. Description

A user may vote for a witness or proposal directly (using an operation: account\_witness\_vote\_operation or update\_proposal\_votes\_operation, //45)  or indirectly (using the proxy - operation:  account\_witness\_proxy\_operation, // 13).    

If a user sets a proxy, the witness votes are removed and the proposal votes are deactivated.    
If a user removes a proxy, there are no witness votes and the proposal votes are activated.    
Settings proxy means that a user wants to cede a decision on which witnesses to vote for to an account indicated as {proxy}. {proxy} will also vote for proposals in the name of {account}.    
If the operation account\_witness\_vote\_operation or account\_witness\_proxy\_operation or update\_proposal\_votes\_operation is not executed in HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, the votes are removed and the virtual operation: expired\_account\_notification\_operation is generated. If the proxy was set and now it is removed, the additionally the virtual operation: proxy\_cleared\_operation is generated.    

The global parameters:     
\- HIVE\_GOVERNANCE\_VOTE\_EXPIRATION\_PERIOD, default mainnet value: 365 days

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| account        |                                                                                                                |              |
| proxy = HIVE\_PROXY\_TO\_SELF\_ACCOUNT; | Account name <br>To remove the proxy, the parameter should be set empty.<br>Only one proxy may be set for an account. |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:     

\- vote right should not be declined (operation decline\_voting\_rights\_operation, // 36).    

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:     

\- the proxy is added/removed. When {proxy} is removed, {account} will have no active witness votes and the proposal votes are activated (if there were any before setting proxy).     

\- RC cost is paid by {account}.    

\- if the proxy is changed or removed, the virtual operation: proxy\_cleared\_operation is generated.