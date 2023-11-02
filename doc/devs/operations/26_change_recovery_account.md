# change\_recovery\_account\_operation, // 26

## 1. Description

The operation change\_recovery\_account\_operation allows a user to update their recovery account. It is important to keep it actual, because only a recovery account may create a request account recovery in case of compromised the owner authority.

By default the recovery account is set to the account creator or it is empty if it was created by temp account or mined.

In order to cancel the change\_recovery\_account\_operation, the operation change\_recovery\_account\_operation, the operation should be created with {new\_recovery\_account} set to the old one.

The operation is done with a 30 days (HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD) delay. 

Global parameter:   
\-  HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD - default mainnet value: 30 days

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| account\_to\_recover   | The account that would be recovered in case of compromise                                                                                                                                                    |         |
| new\_recovery\_account | A new recovery account - the account that creates the recovery request.If a user sets a recovery account as null or their account\_name, it would be impossible to process the account recovery effectively. |         |
| extensions             | Not currently used.                                                                                                                                                                                          |         |

## 3. Authority

Owner

## 4. Operation preconditions

You need:   

\-  enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:   

\-  after 30 days, the recovery account will be updated.   

\-  when the recovery account is updated, the virtual operation: changed\_recovery\_account\_operation is generated.   

\-  RC cost is paid by { account\_to\_recover}.
