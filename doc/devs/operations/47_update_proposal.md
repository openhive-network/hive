# update\_proposal\_operation, // 47

## 1. Description

A user who created the proposal may update it. A user may decrease {daily\_pay}, change subject, permlink and {end\_date} (using {extension}).

In order to update the proposal parameters, all parameters should be entered.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| proposal\_id   | Proposal id                                                                                                             |         |
| creator        | {creator} of the proposal (see operation: create\_proposal\_operation}                                                  |         |
| daily\_pay     | Amount of HBDs to be paid daily to the {receiver} account.Updated value has to be lower or equal to the current amount. |         |
| subject        | Proposal subject                                                                                                        |         |
| permlink       | The identifier of the post created by {creator} or {receiver}.                                                          |         |
| extensions     | The {end\_date} may be updated, but it can be only shortened.                                                           |         |

## 3. Authority

Active


## 4. Operation preconditions

You need:   

\- a user has to be the creator of the proposal.   

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state 

After the operation:   

\- the proposal is updated.   

\- RC cost is paid by { creator }.