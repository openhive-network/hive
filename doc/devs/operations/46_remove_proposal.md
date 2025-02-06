# remove\_proposal\_operation, // 46

## 1. Description

Using operation remove\_proposal\_operation, a user may remove proposals specified by given IDs.

## 2. Parameters

| Parameter name | Description | Example |
| --------------- | ----------------------------------------------------------------------------------------------------------|-------- |
| Parameter name  | Description                                                                                               |         |
| proposal\_owner | {creator} of the proposal (see operation: create\_proposal\_operation}                                    |         |
| proposal\_ids   | IDs of proposals to be removed. Before HF28 nonexisting IDs are ignored from HF28 they trigger an error.  |         |
| extensions      | Not currently used.                                                                                       |         |


## 3. Authority

Active

## 4. Operation preconditions

You need:   

\- a user should be the { creator } of the proposal.   

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- the proposal is removed from the list of proposals.   

\- RC cost is paid by { account }.