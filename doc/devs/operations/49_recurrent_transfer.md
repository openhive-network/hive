# Recurrent transfer - recurrent\_transfer\_operation, // 49

## 1. Description

Creates/updates/removes a recurrent transfer in the currency Hive or HBD.

Since HF 28, if user has more than one recurrent transfer to the same receiver or if user creates the recurrent transfer using pair_id in the {extensions}, user has to specify the pair_id in order to update or remove the defined recurrent transfer. 

\- If amount is set to 0, the recurrent transfer will be deleted and the virtual operation fill_recurrent_transfer_operation is not generated.

\- If there is already a recurrent transfer matching from and to, the recurrent transfer will be replaced, but:

\- if the {recurrence} is not changed, the next execution will be according to “old definition”. 

\- if the {recurrence} is changed, then the next execution will be “update date” + {recurrence} There is no execution on the update date. 

\- Up to HF28 there can be only one recurrent transfer for sender {from} and receiver {to}. Since H28 users may define more recurrent transfers to the same sender and receiver using pair_id in the {executions}.

\- The one account may define up to 255 recurrent transfers to other accounts. 

\- The execution date of the last transfer should be no more than 730 days in the future.


The global parameters:

\- HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES, default mainnet value: 10

\- HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE, default mainnet value: 24

\- HIVE_MAX_RECURRENT_TRANSFER_END_DATE, default mainnet value: 2 years in days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| from           |                                                                           |              |
| to             | Account to transfer asset to. Cannot set a transfer to yourself                                                                             |              |
| amount         | The amount of asset to transfer from @ref from to @ref to. <br> If the recurrent transfer failed 10  (HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES) times because of the lack of funds, the recurrent transfer will be deleted. <br> Allowed currency: Hive and HBD. |  |
| memo           | must be shorter than 2048 |              |
| recurrence = 0; | How often will the payment be triggered, unit: hours.<br> The first transfer is executed immediately.<br> The minimum value of the parameter is 24 h. |              |
| executions = 0; | How many times the recurrent payment will be executed.<br> Executions must be at least 2, if you set executions to 1 the recurrent transfer will not be executed. |              |
| extensions | Extensions.<br> Since HF 28 it may contain the {pair_id} - it allows to define more than one recurrent transfer from sender to the same receiver {to}.<br> Default value {pair_id=0}. |              |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive/HBD is equal or higher than the first recurrent transfer.

\- enough Resource Credit (RC)  to make an operation - for all defined by operation recurrent transfers.  


## 5. Impacted state

After the operation:

\- {from} account balance is reduced by the transferred {amount} if the transfer is .

\- {to} account balance is increased by the transferred {amount} if the transfer is executed.

\- RC cost is paid by {from} account once in advance.

\- every {recurrence} hours a new recurrent transfer will be executed, till it will be executed {executions} times.

\- when the recurrent transfer is executed, the virtual operation: fill_recurrent_transfer_operation is generated. 

\- if the first transfer fails because of the lack of funds, it will not be automatically repeated. 

\- if the recurrent transfer was executed at least one time and the generation of recurrent transfer fails because of the lack of funds, the virtual operation: failed_recurrent_transfer_operation is generated. 
