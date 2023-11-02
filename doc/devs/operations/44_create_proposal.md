# create\_proposal\_operation, // 44

## 1. Description

There is a Decentralized Hive Fund (DHF) on the Hive. Users may submit proposals for funding and if the proposal receives enough votes, it will be funded.

In order to create a proposal user should create a post first and then marked it asas a proposal with the operation create\_proposal\_operation. User defines when the proposal starts and ends and how many funds need to realize it.

The creating proposal costs 10 HBD and additionally 1 HBD for each day over 60 days. The fee goes back to DHF.

Every hour all active proposals are processed and taking into consideration a current number of votes payments are done. Accounts can create/remove votes anytime.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------------------------------------- | ------------ |
| creator        | An account that creates the proposal                                                         |         |
| receiver       | Account to be paid if given proposal has sufficient votes                                    |         |
| start\_date    | When the proposal starts                                                                     |         |
| end\_date      | When the proposal ends                                                                       |         |
| daily\_pay     | Amount of HBD to be daily paid to the { receiver} account.                                   |         |
| subject        |  Proposal subject                                                                            |         |
| permlink       | Given link shall be a valid permlink. Must be posted by the { creator } or the { receiver }. |         |
| extensions     |  Not currently used.                                                                         |         |

## 3. Authority

Active

## 4. Operation preconditions

You need:    

\- the post should be created by { creator } or { receiver }.    

\- enough  HBD to pay the fee.    

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:   

\- the fee 10 HBD is charged from { creator } - when the fee is charged, the virtual operation proposal\_fee\_operation is generated.   

\- if the proposal lasts more than 60 days, the 1 HBD is charged for each day.

\- the proposal is published.

\- if the proposal receives enough votes, the { receiver } receives { daily\_pay } HBD. The {daily\_pay/24} is transferred every hour - when it is transferred, the virtual operation: proposal\_pay is generated.

\- if the proposal doesn’t receive enough votes or a treasury account hasn't enough HBD, the {receiver} may not receive any HBD or may be paid partially.

\- RC cost is paid by { creator }.
