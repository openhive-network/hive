
<a name="_modulesmd"></a>

# @hive/beekeeper

## Interfaces

- [IBeekeeperInfo](#interfacesibeekeeperinfomd)
- [IBeekeeperInstance](#interfacesibeekeeperinstancemd)
- [IBeekeeperOptions](#interfacesibeekeeperoptionsmd)
- [IBeekeeperSession](#interfacesibeekeepersessionmd)
- [IBeekeeperUnlockedWallet](#interfacesibeekeeperunlockedwalletmd)
- [IBeekeeperWallet](#interfacesibeekeeperwalletmd)
- [IWallet](#interfacesiwalletmd)
- [IWalletCreated](#interfacesiwalletcreatedmd)

## Type Aliases

### TPublicKey

Ƭ **TPublicKey**: `string`

#### Defined in

src/interfaces.ts:4

___

### TSignature

Ƭ **TSignature**: `string`

#### Defined in

src/interfaces.ts:5

## Variables

### DEFAULT\_STORAGE\_ROOT

• `Const` **DEFAULT\_STORAGE\_ROOT**: ``"/storage_root"``

#### Defined in

src/web.ts:10

## Functions

### default

▸ **default**(`...args`): `Promise`\<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Creates a Beekeeper instance able to own sessions and wallets

#### Parameters

| Name | Type |
| :------ | :------ |
| `...args` | [options: Partial\<IBeekeeperOptions\>] |

#### Returns

`Promise`\<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Beekeeper API Instance

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

node_modules/.pnpm/typescript@5.2.2/node_modules/typescript/lib/lib.es5.d.ts:365


<a name="interfacesibeekeeperinfomd"></a>

# Interface: IBeekeeperInfo

## Properties

### now

• **now**: `string`

Current server's time

Note: Time is in format: YYYY-MM-DDTHH:mm:ss

#### Defined in

src/interfaces.ts:34

___

### timeout\_time

• **timeout\_time**: `string`

Time when wallets will be automatically closed

Note: Time is in format: YYYY-MM-DDTHH:mm:ss

#### Defined in

src/interfaces.ts:43


<a name="interfacesibeekeeperinstancemd"></a>

# Interface: IBeekeeperInstance

## Methods

### createSession

▸ **createSession**(`salt`): [`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Creation of a session

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `salt` | `string` | a salt used for creation of a token |

#### Returns

[`IBeekeeperSession`](#interfacesibeekeepersessionmd)

a beekeeper session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:261

___

### delete

▸ **delete**(): `Promise`\<`void`\>

Locks all of the unlocked wallets, closes them, closes opened sessions and deletes the current Beekeeper API instance making it unusable

#### Returns

`Promise`\<`void`\>

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:268


<a name="interfacesibeekeeperoptionsmd"></a>

# Interface: IBeekeeperOptions

## Properties

### enableLogs

• **enableLogs**: `boolean`

Whether logs can be written. By default logs are enabled

**`Default`**

```ts
true
```

#### Defined in

src/interfaces.ts:61

___

### storageRoot

• **storageRoot**: `string`

The path of the wallet files (absolute path or relative to application data dir). Parent of the `.beekeeper` directory

Defaults to "/storage_root" for web and "." for web

#### Defined in

src/interfaces.ts:54

___

### unlockTimeout

• **unlockTimeout**: `number`

Timeout for unlocked wallet in seconds (default 900 - 15 minutes).
Wallets will automatically lock after specified number of seconds of inactivity.
Activity is defined as any wallet command e.g. list-wallets

**`Default`**

```ts
900
```

#### Defined in

src/interfaces.ts:71


<a name="interfacesibeekeepersessionmd"></a>

# Interface: IBeekeeperSession

## Methods

### close

▸ **close**(): [`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)

Locks all of the unlocked wallets, closes them, closes this session and makes it unusable

#### Returns

[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)

Beekeeper instance owning the closed session

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:248

___

### createWallet

▸ **createWallet**(`name`, `password?`): `Promise`\<[`IWalletCreated`](#interfacesiwalletcreatedmd)\>

Creates a new Beekeeper wallet object owned by this session

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `name` | `string` | name of wallet |
| `password?` | `string` | password used for creation of a wallet. Not required and in this case a password is automatically generated and returned |

#### Returns

`Promise`\<[`IWalletCreated`](#interfacesiwalletcreatedmd)\>

the created unlocked Beekeeper wallet object

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:219

___

### getInfo

▸ **getInfo**(): [`IBeekeeperInfo`](#interfacesibeekeeperinfomd)

Retrieves the current session info

#### Returns

[`IBeekeeperInfo`](#interfacesibeekeeperinfomd)

Current session information

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:198

___

### listWallets

▸ **listWallets**(): [`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]

Lists all of the opened wallets

#### Returns

[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]

array of opened Beekeeper wallets (either unlocked or locked)

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:207

___

### lockAll

▸ **lockAll**(): [`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]

Locks all of the unlocked wallets owned by this session

#### Returns

[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]

array of the locked Beekeeper wallets

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:239

___

### openWallet

▸ **openWallet**(`name`): [`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)

Opens Beekeeper wallet object owned by this session

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `name` | `string` | name of wallet |

#### Returns

[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)

the opened Beekeeper wallet object (may be unlocked if it has been previously unlocked)

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:230


<a name="interfacesibeekeeperunlockedwalletmd"></a>

# Interface: IBeekeeperUnlockedWallet

## Hierarchy

- [`IWallet`](#interfacesiwalletmd)

  ↳ **`IBeekeeperUnlockedWallet`**

## Properties

### name

• `Readonly` **name**: `string`

Name of this wallet

#### Inherited from

[IWallet](#interfacesiwalletmd).[name](#name)

#### Defined in

src/interfaces.ts:23

## Methods

### close

▸ **close**(): [`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Ensures that this wallet is locked, then closes it

#### Returns

[`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Beekeeper session owning the closed wallet

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Inherited from

[IWallet](#interfacesiwalletmd).[close](#close)

#### Defined in

src/interfaces.ts:15

___

### decryptData

▸ **decryptData**(`content`, `key`, `anotherKey?`): `string`

Decrypts given data from a specific entity and returns the decrypted message

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `content` | `string` | Base58 content to be decrypted |
| `key` | `string` | public key to find the private key in the wallet and decrypt the data |
| `anotherKey?` | `string` | other public key to find the private key in the wallet and decrypt the data (optional - use if the message was encrypted for somebody else) |

#### Returns

`string`

decrypted buffer

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:141

___

### encryptData

▸ **encryptData**(`content`, `key`, `anotherKey?`): `string`

Encrypts given data for a specific entity and returns the encrypted message

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `content` | `string` | Content to be encrypted |
| `key` | `string` | public key to find the private key in the wallet and encrypt the data |
| `anotherKey?` | `string` | other public key to find the private key in the wallet and encrypt the data (optional - use if the message is to encrypt for somebody else) |

#### Returns

`string`

base58 encrypted buffer

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:128

___

### getPublicKeys

▸ **getPublicKeys**(): `string`[]

Lists all of the public keys

#### Returns

`string`[]

a set of all keys for all unlocked wallets

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:150

___

### importKey

▸ **importKey**(`wifKey`): `Promise`\<`string`\>

Imports given private key to this wallet

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `wifKey` | `string` | private key in WIF format to import |

#### Returns

`Promise`\<`string`\>

Public key generated from the imported private key

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:93

___

### lock

▸ **lock**(): [`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)

Locks the current wallet

#### Returns

[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)

Locked beekeeper wallet

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:82

___

### removeKey

▸ **removeKey**(`publicKey`): `Promise`\<`void`\>

Removes given key from this wallet

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `publicKey` | `string` | public key in WIF format to match the private key in the wallet to remove |

#### Returns

`Promise`\<`void`\>

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:103

___

### signDigest

▸ **signDigest**(`publicKey`, `sigDigest`): `string`

Signs a transaction by signing a digest of the transaction

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `publicKey` | `string` | public key in WIF format to match the private key in the wallet. It will be used to sign the provided data |
| `sigDigest` | `string` | digest of a transaction in hex format |

#### Returns

`string`

signed data in hex format

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:115


<a name="interfacesibeekeeperwalletmd"></a>

# Interface: IBeekeeperWallet

## Hierarchy

- [`IWallet`](#interfacesiwalletmd)

  ↳ **`IBeekeeperWallet`**

## Properties

### name

• `Readonly` **name**: `string`

Name of this wallet

#### Inherited from

[IWallet](#interfacesiwalletmd).[name](#name)

#### Defined in

src/interfaces.ts:23

___

### unlocked

• `Optional` `Readonly` **unlocked**: [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Indicates if the wallet is unlocked. If the wallet is locked, this property will be undefined. IBeekeeperUnlockedWallet type otherwise

#### Defined in

src/interfaces.ts:171

## Methods

### close

▸ **close**(): [`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Ensures that this wallet is locked, then closes it

#### Returns

[`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Beekeeper session owning the closed wallet

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Inherited from

[IWallet](#interfacesiwalletmd).[close](#close)

#### Defined in

src/interfaces.ts:15

___

### unlock

▸ **unlock**(`password`): [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Unlocks this wallet

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `password` | `string` | password to the wallet |

#### Returns

[`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Unlocked Beekeeper wallet

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:163


<a name="interfacesiwalletmd"></a>

# Interface: IWallet

## Hierarchy

- **`IWallet`**

  ↳ [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

  ↳ [`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)

## Properties

### name

• `Readonly` **name**: `string`

Name of this wallet

#### Defined in

src/interfaces.ts:23

## Methods

### close

▸ **close**(): [`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Ensures that this wallet is locked, then closes it

#### Returns

[`IBeekeeperSession`](#interfacesibeekeepersessionmd)

Beekeeper session owning the closed wallet

**`Throws`**

on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Defined in

src/interfaces.ts:15


<a name="interfacesiwalletcreatedmd"></a>

# Interface: IWalletCreated

## Properties

### password

• **password**: `string`

Password used for unlocking your wallet

#### Defined in

src/interfaces.ts:187

___

### wallet

• **wallet**: [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Unlocked, ready to use wallet

#### Defined in

src/interfaces.ts:180
