# witness\_set\_properties\_operation, // 42

## 1. Description

This is an operation for witnesses.

This is one of the two operations allowing to update witness properties (see witness\_update\_operation). The price feed (hbd\_exchange\_rate) can also be set with feed\_publish\_operation.

The whole list of properties, with their meaning, units and allowed values, is in [witness\_parameters.md](../../witness_parameters.md).

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| owner          | witness                                                                                                                                                                                                                             |         |
| props          | A list of [name, value] pairs. `key` (the witness's current signing key) is required; the others are optional and only the ones given are changed: account\_creation\_fee, account\_subsidy\_budget, account\_subsidy\_decay, maximum\_block\_size, hbd\_interest\_rate, hbd\_exchange\_rate, url and new\_signing\_key. Each value is binary encoded and written as a hex string, see below. |         |
| extensions     |                                                                                                                                                                                                                                     |         |

### How props values are encoded

Unlike other operations, the values in `props` are not plain JSON values. Each value is the binary serialization of the property, written as a hex string. Integers are little-endian. In the binary form of the operation (the one that is signed) the pairs are ordered by name, so a client that serializes the operation itself must sort them. Where the table shows a value in several parts, the parts are written one after another without spaces.

| Property | Type | Encoding | Example value | Hex |
| -------- | ---- | -------- | ------------- | --- |
| key, new\_signing\_key | public key | 33 bytes: the compressed public key. Base58-decode the part after `STM` (37 bytes) and drop the last 4 bytes (the checksum). | `STM5dLh5HxjjawY4Gm6o6ugmJUmEXgnfXXXRJPRTxRnvfFBJ24c1M` | `02614c335eae65faadc307003549cf491126a883c10a57d918c651b9040ed82616` |
| account\_creation\_fee | asset | 8 bytes amount in the smallest unit (0.001 HIVE), then the symbol, in one of two forms (see below) | 1.000 HIVE | `e803000000000000` `03535445454d0000` or `e803000000000000` `2320bcbe` |
| maximum\_block\_size | uint32 | 4 bytes | 65536 | `00000100` |
| hbd\_interest\_rate | uint16 | 2 bytes, in hundredths of a percent | 800 (8%) | `2003` |
| account\_subsidy\_budget | int32 | 4 bytes | 797 | `1d030000` |
| account\_subsidy\_decay | uint32 | 4 bytes | 347321 | `b94c0500` |
| hbd\_exchange\_rate | price | `base` asset (HBD) then `quote` asset (HIVE), each encoded like account\_creation\_fee | 0.034 HBD / 1.000 HIVE | `2200000000000000` `0353424400000000` `e803000000000000` `03535445454d0000` |
| url | string | length (as a varint), then the UTF-8 bytes | https://testurl | `0f68747470733a2f2f7465737475726c` |

The asset symbol is accepted in either of two forms:

- legacy, 8 bytes: the precision (`03`) followed by the old symbol name padded with zero bytes, `STEEM` for HIVE and `SBD` for HBD (`TESTS` and `TBD` on testnet): `03535445454d0000` for HIVE, `0353424400000000` for HBD;
- NAI, 4 bytes: the asset number, `2320bcbe` for HIVE (3200000035) and `0320bcbe` for HBD (3200000003).

wax produces the NAI form; dhive and hive-js produce the legacy form.

Values are decoded leniently, so check them before signing. A hex string is read two characters per byte: an odd final character is dropped without being checked, and any other character that is not a hex digit is rejected; bytes after the end of the value are ignored; a value too short for its type is rejected, but an empty one is not rejected by the decoding, only by the property's own checks, if any. In particular a one-character value such as `"8"` becomes empty, and for `hbd_interest_rate`, where no check catches it, that sets the code default `1000` (10%).

Example: an operation that sets the HBD interest rate to 8%:

```json
{
  "type": "witness_set_properties_operation",
  "value": {
    "owner": "witness-account",
    "props": [
      ["hbd_interest_rate", "2003"],
      ["key", "<current signing key, 33 bytes as hex>"]
    ],
    "extensions": []
  }
}
```

Property names that the chain does not recognize are ignored without an error, so a misspelled name produces a valid operation that changes nothing. The legacy names `sbd_interest_rate` and `sbd_exchange_rate` are also accepted; if both the legacy and the current name are given, only the legacy one is checked and used.

Client libraries build these values for you: wax (`WitnessSetPropertiesOperation`, field `hbdInterestRate`) and dhive and hive-js (`buildWitnessUpdateOp`, field `hbd_interest_rate`). All of them take the interest rate as the raw number (`800` for 8%), not as a percent. dhive and hive-js reject unknown property names.

## 3. Authority

The witness signing key given in `props["key"]`. No owner, active or posting authority is used.

## 4. Operation preconditions

You need:    

\- { owner } is a witness.   

\- `props["key"]` is the witness's current signing key (the one published on chain).   

\- enough Resource Credit (RC) to make an operation.


## 5. Impacted state

After the operation:    

\- the properties are updated.    

\- RC cost is paid by { owner }.
