# Power up - transfer\_to\_vesting\_operation, // 3

## 1.  Description

The operation is also called Staking. This operation converts Hive into Hive Power (also called Vesting Fund Shares  or VESTS) at the current exchange rate.

The conversion may be done between the same account or to another account.

The more HP (Hive Power) the account has, the more:

a. Governance voting power (for witnesses and proposals) has

b. Social voting power has (indirectly affects Increase curation rewards)

c. Resource Credit has

The global parameters:

- HIVE\_DELAYED\_VOTING\_TOTAL\_INTERVAL\_SECONDS, default mainnet value: 30 days
- HIVE\_DELAYED\_VOTING\_INTERVAL\_SECONDS, default mainnet value: 30 days 1 day


## 2.  Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ------------------------------------------------------------------ | ------------ |
| from           | The account the funds are coming from                              |              |
| to             | The account the funds are going to. If empty string, then the same as from |              |
| amount         | Must be HIVE, amount >0                                            | 100.000 HIVE |


## 3.  Authority

Active


## 4.  Operation preconditions

You need:

\- your balance in HIVE is equal or higher than transfer amount.

\- enough Resource Credit (RC)  to make an operation.


## 5.  Impacted state

After the operation:

\- {from} account balance is reduced by transferred amount

\- {to} account vesting balance is increased by the transferred amount, but the Governance voting power is increased after 30 days.

\- the virtual operation: transfer\_to\_vesting\_completed is created. The virtual operation contains information about the amount of received Vest. 

\- after 30 days, the virtual operation: delayed\_voting\_operation is generated.

\- RC max\_mana is increased.

\- RC cost is paid by {from} account (RC current\_mana is reduced).
