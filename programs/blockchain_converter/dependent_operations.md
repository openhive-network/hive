# Dependent operations

This file describes what information given operations require in order for it to work properly

## Operations

### hive operations

Hive operations derived from the base operation - "physical" operations available in the standard hive build:

<details>
<summary>0. `vote_operation`</summary>

#### Dependent members

* [`voter`](#account_name_type)
* [`author`](#account_name_type)
* [`permlink`](#permlink-aka-string)

</details>

<details>
<summary>1. `comment_operation`</summary>

#### Dependent members

* [`parent_author`](#account_name_type)
* [`parent_permlink`](#permlink-aka-string)
* [`author`](#account_name_type)

</details>

## Resolving type dependencies

### `account_name_type`

Accounts must be created before referencing them, so we should create them using such operation as `account_create_operation`.

### `permlink` aka `string`

Permlinks can be created using `comment_operation`
