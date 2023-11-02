# account\_update\_operation, // 10

## 1. Description

There are two operations that allow updating an account data: account\_update2\_operation and account\_update\_operation.

Operations account\_update\_operation and account\_update2\_operation share a limit of allowed updates of the owner authority  - two executions per 60 minutes (HIVE\_OWNER\_UPDATE\_LIMIT) - meaning each of them can be executed twice or both can be executed once during that time period. 

After 30 days (HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD) using the account recovery process to change the owner authority is no longer possible.

The operation account\_update\_operation allows changing authorities, it doesn’t allow changing the posting\_json\_metadata. 

Global parameters:

- HIVE\_OWNER\_UPDATE\_LIMIT , default mainnet value: 60 minutes,
- HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD, default mainnet value: 30 days.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| account        | account\_name\_type, it cannot be updated.                                                                                                                                                                                                                                            |         |
| owner          | optional< authority >In order to update the {owner}, the owner authority is required.It may be changed 2 times per hour.If a user provides a new authority, the old one will be deleted, but the deleted authority may be used up to 30 days in the process of the recovery account.  |         |
| active         | optional< authority >In order to update the {active}, the active authority is required.If a user provides a new authority, the old one will be deleted.                                                                                                                               |         |
| posting.       | optional< authority >In order to update the {posting}, the active authority is required.If a user provides a new authority, the old one will be deleted.                                                                                                                              |         |
| memo\_key      | optional< public\_key\_type >In order to update the {memo\_key}, active authority is required.If a user provides a new key, the old one will be deleted.                                                                                                                              |         |
| json\_metadata | json\_stringIn order to update the {json\_metadata}, the active authority is required                                                                                                                                                                                                 |         |


## 3. Authority

You need the owner authority if you want to update the {owner}.

You need the active (or owner) authority to update the {active}, {posting}, { memo\_key} or {json\_metadata}.


## 4. Operation preconditions

You need:

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:

\- the parameter/parameters are updated.

\- RC cost is paid by { account }.
