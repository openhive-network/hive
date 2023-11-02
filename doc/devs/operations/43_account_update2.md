# account\_update2\_operation, // 43

## 1. Description

There are two operations that allow updating an account data: account\_update\_operation and account\_update2\_operation.

Operations account\_update\_operation and account\_update2\_operation share a limit of allowed updates of the owner authority  - two executions per 60 minutes (HIVE\_OWNER\_UPDATE\_LIMIT) - meaning each of them can be executed twice or both can be executed once during that time period.

After 30 days (HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD) using the account recovery process to change the owner authority is no longer possible. The operation allows to update authority, json\_metadata and the posting\_json\_metadata. Depending on what the user wants to change, a different authority has to be used. 

Each authority (owner, active, posting, memo\_key) consists of:   
\- weight\_threshold   
\- key or account name with its weight.   

The authority may have more than one key and more than one assigned account name.   


Example 1:    
The posting authority:    
weight\_threshold = 1    
“first\_key”, weight =1    
“second\_key”, weight = 1    
“account\_name\_1”, weight =1    

The above settings mean that a user with “first\_key”, a user with “second\_key” or a user “account\_name\_1” may post on behalf of this account.   

Example 2:    
The posting authority:     
weight\_threshold = 2    
“first\_key”, weight =1    
“second\_key”, weight = 1    
“account\_name\_1”, weight =1    
The above settings mean that at least two signatures are needed to post on behalf of this account.

Global parameters:   
\- HIVE\_OWNER\_UPDATE\_LIMIT, default mainnet value: 60 minutes    
\- HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD, default mainnet value: 30 days

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------------------------------------- | ------- |
| account        | account\_name\_type, it cannot be updated.                                                        |         |
| owner          | optional< authority > <br> In order to update the {owner}, the owner authority is required.<br>It may be changed 2 times per hour.<br>If a user provides a new authority, the old one will be deleted. |         |
| active         | optional< authority > <br> In order to update the {active}, the active authority is required.<br>If a user provides a new authority, the old one will be deleted.                                  |         |
| posting        | optional< authority > <br> In order to update the {posting}, the active authority is required.<br>If a user provides a new authority, the old one will be deleted.                              |         |                             
| memo\_key      | optional< public\_key\_type > <br> In order to update the {memo\_key}, the active authority is required.<br>If a user provides a new key, the old one will be deleted.                             |         |
| json\_metadata | json\_string <br> In order to update the {json\_metadata}, the active authority is required.                      |         |
| posting\_json\_metadata | json\_stringIn order to update the { posting\_json\_metadata }, the posting authority is required.       |         |

## 3.  Authority

You need the owner authority if you want to update the {owner}.

You need the active (or owner) authority to update the {active}, {posting}, { memo\_key} or {json\_metadata}.   

You need the posting (or active or owner) authority to update the { posting\_json\_metadata} for instance about, profile\_image, cover\_image, location, dtube\_pub, website.

## 4. Operation preconditions

You need:

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:   

\- the parameter/parameters are updated.    

\- RC cost is paid by {account}.