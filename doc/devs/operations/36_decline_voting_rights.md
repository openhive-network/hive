# decline\_voting\_rights\_operation, // 36

## 1. Description

Using the operation decline\_voting\_rights\_operation, a user may decide to decline their voting rights – for content, witnesses and proposals. Additionally, a user cannot set a proxy (operation account\_witness\_proxy\_operation, // 13).

The operation is done with a HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD  day delay. After HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD days it is irreversible.

During HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD days after creation, the operation may be canceled using the operation declive\_voting\_rights\_operation with {decline = false}.

Global parameter:

\- HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD - default mainnet value: 30 days

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------ | ------------ |
| account        | Account name                  |         |
| decline        | Default value: decline = true |         | 


## 3. Authority Owner

## 4. Operation preconditions

You need:   

\- enough Resource Credit (RC)  to make an operation.   

\- the decline\_voting\_rights\_operation with { decline = false }, may be created only up to HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD  days after creating decline\_voting\_rights\_operation with decline = true.

## 5. Impacted state

After the operation:   

\- after HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD days, the account { account } has no voting rights (for contents, witnesses, proposals) and it cannot set a proxy.   

\- after HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD days, the virtual operation: declined\_voting\_rights\_operation is generated.- RC cost is paid by { account }.
