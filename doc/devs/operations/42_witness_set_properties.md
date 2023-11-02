# witness\_set\_properties\_operation, // 42

## 1. Description

This is an operation for witnesses.

This is one of the two operations allowing to update witness properties (see witness\_update\_operation).

The whole list of properties is available here: https\://gitlab.syncad.com/hive/hive/-/blob/master/doc/witness\_parameters.md.

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| owner          | witness                                                                                                                                                                                                                             |         |
| props          | There are the following properties available in the { props}: account\_creation\_fee, account\_subsidy\_budget, account\_subsidy\_decay, maximum\_block\_size, hbd\_interest\_rate. hbd\_exchange\_rate,  url and new\_signing\_key |         |
| extensions     |                                                                                                                                                                                                                                     |         |

## 3. Authority

Witness signature

## 4. Operation preconditions

You need:    

\- { owner } is a witness.   

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:    

\- the properties are updated.    

\- RC cost is paid by { owner }.
