# recover\_account\_operation, // 25

## 1. Description

This operation is part of the recovery account process (more information request\_account\_recovery, // 40).

After creating by recovery account the operation request\_account\_recovery, the user has HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD hours to respond using operation recover\_account\_operation and set a new owner authority.

The operation recover\_account\_operation has to be signed using the two owner authorities, the old one (maybe compromised) and the new one (see request\_account\_recovery).

There must be at least 60 minutes (HIVE\_OWNER\_UPDATE\_LIMIT) between executions of operation recover\_account\_operation.

Global parameters: 
\- HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD: default mainnet value: 30 days   
\- HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD: default mainnet value: 1 day   
\- HIVE\_OWNER\_UPDATE\_LIMIT: default mainnet value: 60 minutes    

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| account\_to\_recover     | The account to be recovered                                                                                                                                           |         |
| new\_owner\_authority    | The new owner authority as specified in the request account recovery operation.                                                                                       |         |
| recent\_owner\_authority | A previous owner's authority, may be compromised.If the operation change\_recovery\_account\_operation was generated, it has not been yet 30 days since its creation. |         |
| extensions               | Not currently used.                                                                                                                                                   |         |

## 3. Authority

Two owner authorities are required – the old one and the new one.

## 4. Operation preconditions

You need:

\- the operation request\_account\_recovery should exist and be generated during the HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD.

\- if there is only one operation in the transaction and the authority contains one key, the operation is free. Otherwise enough Resource Credit (RC)  to make an operation.

## 5. Impacted state

After the operation:   

\- the only valid owner authority is the new owner authority.   

\- RC cost is paid by { account\_to\_recover} if required. 
