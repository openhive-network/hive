# Collateralized convert - collateralized\_convert\_operation, // 48

## 1. Description

Similar to convert\_operation, this operation instructs the blockchain to convert HIVE to HBD.

The operation is performed after 3.5 days, but the owner gets HBD  immediately. The price risk is cushioned by extra HIVE (HIVE\_COLLATERAL\_RATIO = 200 % ). After actual conversion takes place the excess HIVE is returned to the owner.

The global parameters:

\- HIVE\_COLLATERALIZED\_CONVERSION\_DELAY, default mainnet value: 3.5 days   
\- HIVE\_COLLATERAL\_RATIO, default mainnet value: 200 %   
\- HIVE\_COLLATERALIZED\_CONVERSION\_FEE, default mainnet value: 5 %    


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ----------------------------------------------------------- | ------- |
| owner          | account\_name\_type                                         |         |
| requestid = 0; | The number is given by a user. Should be unique for a user. |         |
| amount         | Amount > 0, have to be in Hive                              |         |


## 3. Authority

Active


## 4. Operation preconditions

You need:

\- your balance in Hive is equal or higher than the operation amount.

\- enough Resource Credit (RC)  to make an operation.


## 5.  Impacted state

After the operation:

\- immediately, the {owner} account balance in HIVE is reduced by the operation {amount}.

\- immediately, the first part of the {amount} is sent to collateral and the second part is exchanged on the minimal exchange rate from the last 3.5 days decreased by fee (5%), so  {owner} account balance in HBD is increased by the exchanged amount.

\- immediately, the virtual operation collateralized\_convert\_immediate\_conversion\_operation is generated.

\- 3.5 days after the operation, the HBD amount is recalculated using actual  median feed price with applied fee (5%). The difference between {amount} and recalculated amount increases the balance in Hive.

\- when the conversion is executed (after 3.5 days), the virtual operation: fill\_collateralized\_convert\_request\_operation is generated. 

\- RC cost is paid by {owner} account.

**Example**

A user wants to convert HIVE to HBD. {amount} = 200 000 Hive.

HIVE\_COLLATERAL\_RATIO = 200 %

FEE = 5%

Minimal feed price = every hour the median price of the feed price provided by the witnesses (top twenty) is calculated. The minimum of the median provided during the last 3.5 days is Minimal feed price.

Median feed price = every hour the median price of the feed price provided by the witnesses (top twenty) is calculated. The median of the median provided during the last  3,5 days is Median feed price.

Minimal feed price = 0,400

Median feed price = 0,407

Median feed price after 3.5 days =  0, 404

**Before an operation:**

**Balance Hive**: 200 000 Hive

**Balance HBD**: 0 HBD

**Immediately after the operation:**

- {amount} is divided into two parts (because HIVE\_COLLATERAL\_RATIO = 200 %), one is sent to collateral, second is exchanged

**Balance Hive**: 200 000 Hive - 200 000 = 0 Hive

**Collateral**:  100 000 Hive

- Minimal feed price is used to exchange, the fee is applied to the exchange rate.

The minimal feed price: 0,400, so 400 HBD for 1000 Hive.

The 5% fee is applied: 400 HBD for **1050** Hive

HBD amount: 100 000 \* 400 / 1050 = 38 095,2 HBD

**Balance HBD**: 0 + 38 095,2 HBD = 38 095,2 HBD

**3.5 days after the operation:**

- Current median feed price is used to exchange, the fee is applied to the exchange rate.

Median feed price =  0, 404,  so 404 HBD for 1000 Hive.

The 5% fee is applied: 404 HBD for **1050** Hive

Hive amount: 38 095,2 \* 1050 / 404 = 99009,90

-  The calculated amount is deducted from {amount}

200 000 - 99009,90 = 100990,01 HIVE

**Balance Hive**: 0+100990,01  = 100990,01 HIVE

**Collateral**:0 HIVE

**Balance HBD:** 38 095,2 HBD
