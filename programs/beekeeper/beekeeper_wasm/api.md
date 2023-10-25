
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

src/interfaces.ts:1

___

### TSignature

Ƭ **TSignature**: `string`

#### Defined in

src/interfaces.ts:2

## Functions

### default

▸ **default**(`options?`): `Promise`<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Creates a Beekeeper instance able to own sessions and wallets

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `options` | `Partial`<[`IBeekeeperOptions`](#interfacesibeekeeperoptionsmd)\> | options passed to the WASM Beekeeper |

#### Returns

`Promise`<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Beekeeper API Instance

#### Defined in

src/detailed/beekeeper.ts:20


<a name="interfacesibeekeeperinfomd"></a>

# Interface: IBeekeeperInfo

## Properties

### now

• **now**: `string`

Current server's time

Note: Time is in format: YYYY-MM-DDTHH:mm:ss

#### Defined in

src/interfaces.ts:31

___

### timeout\_time

• **timeout\_time**: `string`

Time when wallets will be automatically closed

Note: Time is in format: YYYY-MM-DDTHH:mm:ss

#### Defined in

src/interfaces.ts:40


<a name="interfacesibeekeeperinstancemd"></a>

# Interface: IBeekeeperInstance

## Methods

### createSession

▸ **createSession**(`salt`): `Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Creation of a session

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `salt` | `string` | a salt used for creation of a token |

#### Returns

`Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

a beekeeper session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.

#### Defined in

src/interfaces.ts:220

___

### delete

▸ **delete**(): `Promise`<`void`\>

Locks all of the unlocked wallets, closes them, closes opened sessions and deletes the current Beekeeper API instance making it unusable

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<`void`\>

#### Defined in

src/interfaces.ts:227


<a name="interfacesibeekeeperoptionsmd"></a>

# Interface: IBeekeeperOptions

## Properties

### enableLogs

• **enableLogs**: `boolean`

Whether logs can be written. By default logs are enabled

**`default`** true

#### Defined in

src/interfaces.ts:57

___

### storageRoot

• **storageRoot**: `string`

The path of the wallet files (absolute path or relative to application data dir). Parent of the `.beekeeper` directory

**`default`** "/storage_root"

#### Defined in

src/interfaces.ts:50

___

### unlockTimeout

• **unlockTimeout**: `number`

Timeout for unlocked wallet in seconds (default 900 - 15 minutes).
Wallets will automatically lock after specified number of seconds of inactivity.
Activity is defined as any wallet command e.g. list-wallets

**`default`** 900

#### Defined in

src/interfaces.ts:67


<a name="interfacesibeekeepersessionmd"></a>

# Interface: IBeekeeperSession

## Methods

### close

▸ **close**(): `Promise`<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Locks all of the unlocked wallets, closes them, closes this session and makes it unusable

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperInstance`](#interfacesibeekeeperinstancemd)\>

Beekeeper instance owning the closed session

#### Defined in

src/interfaces.ts:207

___

### createWallet

▸ **createWallet**(`name`, `password?`): `Promise`<[`IWalletCreated`](#interfacesiwalletcreatedmd)\>

Creates a new Beekeeper wallet object owned by this session

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `name` | `string` | name of wallet |
| `password?` | `string` | password used for creation of a wallet. Not required and in this case a password is automatically generated and returned |

#### Returns

`Promise`<[`IWalletCreated`](#interfacesiwalletcreatedmd)\>

the created unlocked Beekeeper wallet object

#### Defined in

src/interfaces.ts:189

___

### getInfo

▸ **getInfo**(): `Promise`<[`IBeekeeperInfo`](#interfacesibeekeeperinfomd)\>

Retrieves the current session info

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperInfo`](#interfacesibeekeeperinfomd)\>

Current session information

#### Defined in

src/interfaces.ts:168

___

### listWallets

▸ **listWallets**(): `Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]\>

Lists all of the opened wallets

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]\>

array of opened Beekeeper wallets (either unlocked or locked)

#### Defined in

src/interfaces.ts:177

___

### lockAll

▸ **lockAll**(): `Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]\>

Locks all of the unlocked wallets owned by this session

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)[]\>

array of the locked Beekeeper wallets

#### Defined in

src/interfaces.ts:198


<a name="interfacesibeekeeperunlockedwalletmd"></a>

# Interface: IBeekeeperUnlockedWallet

## Hierarchy

- [`IWallet`](#interfacesiwalletmd)

  ↳ **`IBeekeeperUnlockedWallet`**

## Properties

### name

• `Readonly` **name**: `string`

Name of this wallet

**`readonly`**

#### Inherited from

[IWallet](#interfacesiwalletmd).[name](#name)

#### Defined in

src/interfaces.ts:20

## Methods

### close

▸ **close**(): `Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Ensures that this wallet is locked, then closes it

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Beekeeper session owning the closed wallet

#### Inherited from

[IWallet](#interfacesiwalletmd).[close](#close)

#### Defined in

src/interfaces.ts:12

___

### getPublicKeys

▸ **getPublicKeys**(): `Promise`<`string`[]\>

Lists all of the public keys

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<`string`[]\>

a set of all keys for all unlocked wallets

#### Defined in

src/interfaces.ts:120

___

### importKey

▸ **importKey**(`wifKey`): `Promise`<`string`\>

Imports given private key to this wallet

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `wifKey` | `string` | private key in WIF format to import |

#### Returns

`Promise`<`string`\>

Public key generated from the imported private key

#### Defined in

src/interfaces.ts:89

___

### lock

▸ **lock**(): `Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)\>

Locks the current wallet

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperWallet`](#interfacesibeekeeperwalletmd)\>

Locked beekeeper wallet

#### Defined in

src/interfaces.ts:78

___

### removeKey

▸ **removeKey**(`password`, `publicKey`): `Promise`<`void`\>

Removes given key from this wallet

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `password` | `string` | password to the wallet |
| `publicKey` | `string` | public key in WIF format to match the private key in the wallet to remove |

#### Returns

`Promise`<`void`\>

#### Defined in

src/interfaces.ts:99

___

### signDigest

▸ **signDigest**(`publicKey`, `sigDigest`): `Promise`<`string`\>

Signs a transaction by signing a digest of the transaction

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `publicKey` | `string` | public key in WIF format to match the private key in the wallet. It will be used to sign the provided data |
| `sigDigest` | `string` | digest of a transaction in hex format |

#### Returns

`Promise`<`string`\>

signed data in hex format

#### Defined in

src/interfaces.ts:111


<a name="interfacesibeekeeperwalletmd"></a>

# Interface: IBeekeeperWallet

## Hierarchy

- [`IWallet`](#interfacesiwalletmd)

  ↳ **`IBeekeeperWallet`**

## Properties

### name

• `Readonly` **name**: `string`

Name of this wallet

**`readonly`**

#### Inherited from

[IWallet](#interfacesiwalletmd).[name](#name)

#### Defined in

src/interfaces.ts:20

___

### unlocked

• `Optional` `Readonly` **unlocked**: [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Indicates if the wallet is unlocked. If the wallet is locked, this property will be undefined. IBeekeeperUnlockedWallet type otherwise

**`readonly`**

#### Defined in

src/interfaces.ts:141

## Methods

### close

▸ **close**(): `Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Ensures that this wallet is locked, then closes it

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Beekeeper session owning the closed wallet

#### Inherited from

[IWallet](#interfacesiwalletmd).[close](#close)

#### Defined in

src/interfaces.ts:12

___

### unlock

▸ **unlock**(`password`): `Promise`<[`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)\>

Unlocks this wallet

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Parameters

| Name | Type | Description |
| :------ | :------ | :------ |
| `password` | `string` | password to the wallet |

#### Returns

`Promise`<[`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)\>

Unlocked Beekeeper wallet

#### Defined in

src/interfaces.ts:133


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

**`readonly`**

#### Defined in

src/interfaces.ts:20

## Methods

### close

▸ **close**(): `Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Ensures that this wallet is locked, then closes it

**`throws`** {import("../errors").BeekeeperError} on any beekeeper API-related error (error parsing response, invalid input, timeout error, fs sync error etc.)

#### Returns

`Promise`<[`IBeekeeperSession`](#interfacesibeekeepersessionmd)\>

Beekeeper session owning the closed wallet

#### Defined in

src/interfaces.ts:12


<a name="interfacesiwalletcreatedmd"></a>

# Interface: IWalletCreated

## Properties

### password

• **password**: `string`

Password used for unlocking your wallet

#### Defined in

src/interfaces.ts:157

___

### wallet

• **wallet**: [`IBeekeeperUnlockedWallet`](#interfacesibeekeeperunlockedwalletmd)

Unlocked, ready to use wallet

#### Defined in

src/interfaces.ts:150
