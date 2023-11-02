# request\_account\_recovery, // 40

## 1. Description

In case of the compromised owner authority, a user may recover it. There are two conditions that have to be fulfilled to do it:

1\. A user should have an actual recovery account.

During an account creation, the account that created a new account is set as a recovery account by default, but it can be changed by the user (using operation change\_recovery\_account\_operation).

If the account was created by account temp, then a recovery account is empty and it is set as a top witness – it is not good a recovery account.

Note: it takes HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD (30 days) after sending change\_recovery\_account\_operation for the new recovery agent to become active. During that period the previous agent remains active for the account.

2\. The compromised owner authority is still recent.

Owner authority is considered recent and remains valid for the purpose of account recovery for HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD (30 days) after it was changed.

Note: look for account\_update\_operation // 10 or account\_update2\_operation // 43 in account history to see when its owner authority was compromised.


### 1.1. The recovery account process

Conditions:

1\. An account { account\_to\_recover } has an actual recovery account.

2\. An account { account\_to\_recover } realizes that someone may have access to its owner key and it is less than 30 days from generating an operation: change\_recovery\_account\_operation.

Steps:

1. A user { account\_to\_recover } asks his recovery account { recovery\_account } to create a request account recovery (outside the blockchain).
2. A recovery account { recovery\_account } creates an operation:  request\_account\_recovery\_operation with {new\_owner\_authority}.
3. A user { account\_to\_recover } creates an operation: recover\_account\_operation using { new\_owner\_authority} and {recent\_owner\_authority} and signing with two signatures (the old one and the new one). A user has HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD  to generate this operation.

In order to cancel a request, a user should create a new request with weight of authority =0.

The operation: request\_account\_recovery is valid HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD  hours and if after HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD  hours there is no response (operation: recover\_account\_operation) it is expired.

Global parameters:\
\- HIVE\_OWNER\_AUTH\_RECOVERY\_PERIOD: default mainnet value: 30 days

\- HIVE\_ACCOUNT\_RECOVERY\_REQUEST\_EXPIRATION\_PERIOD: default mainnet value: 1 day


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------------------------------------------------------------- | ------------ |
| recovery\_account     | The account that may create a request for account recovery.It is important to keep it actual.  |         |
| account\_to\_recover  | The account to be recovered                                                                    |         |
| new\_owner\_authority | The new owner authority – the public, not private key.The new authority should be satisfiable. |         |
| extensions            | Not currently used.                                                                            |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- the { recovery\_account } should be asked (outside the blockchain) to create a recovery request and should have a {new\_owner\_authority}.

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:

\- the request account recovery is created – the user has 24 hours to respond.

\- RC cost is paid by { recovery\_account }.

 
