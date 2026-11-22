# feed\_publish\_operation, // 7

## 1. Description

This is an operation for witnesses. 

The witnesses publish the exchange rate between Hive and HBD. Only the feeds of the witnesses scheduled in the current round (up to 21, see [witness\_parameters.md](../../witness_parameters.md)) that were updated within the last 7 days are used to define the exchange rate.


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------------------------------------------- | ------- |
| publisher      | The witness                                                                        |         |
| exchange\_rate | How many HBD the 1 Hive should costExample:"base":"0.345 HBD","quote":"1.000 HIVE" |         |

## 3. Authority

Active


## 4. Operation preconditions

You need:

\- the operation is allowed for the witnesses only.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the exchange rate is published; it is counted in the hourly median if the { publisher } is among the scheduled witnesses when the median is taken and the feed is less than 7 days old.

\- RC cost is paid by { publisher }.
