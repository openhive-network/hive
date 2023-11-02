# custom\_json\_operation, // 18

## 1. Description

The operation custom\_json\_operation works similar as custom\_operation, but it is designed to be human readable/developer friendly.The custom\_json\_operation is larger than custom\_operation or custom\_binary, that is why it costs more RC.It should be signed as required in { required\_auths } or  { required\_posting\_auths }.

The examples of custom\_json\_operation:

\- reblog

\- muted
 
\- pinned

\- follow


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------- | ------- |
| required\_auths          | flat\_set< account\_name\_type >                     |         |
| required\_posting\_auths | flat\_set< account\_name\_type >                     |         |
| id                       | custom\_id\_typemust be less than 32 characters long |         |
| json                     | json\_stringmust be a proper utf8 JSON string.       |         |

## 3. Authority

Specified by {required\_auths} (active) and/or {required\_posting\_auths} (posting)

## 4. Operation preconditions

You need:

\- enough Resource Credit (RC) to make an operation.

## 5. Impacted state

After the operation:

\- the custom operation is added to the blockchain.
 
\- RC cost is paid by the first account specified on {required\_auths} or {required\_posting\_auths}.
