# Beekeeper

[![npm version](https://badge.fury.io/js/%40hiveio%2Fbeekeeper.svg)](https://badge.fury.io/js/%40hiveio%2Fbeekeeper)
[![CI](https://gitlab.syncad.com/hive/hive/badges/master/pipeline.svg)](https://gitlab.syncad.com/hive/hive/-/pipelines)

Hive Beekeeper functionality exposed to TypeScript/JavaScript environments

## 📥 Installation

This is a [Node.js](https://nodejs.org/en/) (version 20 or higher) module available through the
[npm registry](https://www.npmjs.com/):

```bash
npm install @hiveio/beekeeper
```

Beekeeper is written in TypeScript, so out of the box it gives you clear API spec.

## 💡 Example usage

As a an example you can check [`examples`](https://gitlab.syncad.com/hive/hive/-/tree/master/programs/beekeeper/examples) in this project.

### Importing and removing keys from a wallet

Note: between different beekeeper instances, session names can be repeated, but not the wallet names!

For salts, you should always use strong random values, such as `crypto.randomUUID()`. Also, remember to always use strong passwords for wallets! If you provide `undefined` as a password when creating a wallet, a strong random password will be generated for you and returned in the result.

For simplicity, and as `crypto` API may not be available in all environments, we are using `Math.random()` for salt generation. As for passwords, we are using simple strings. **Do not use this approach in production!**

```js
import beekeeperFactory from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

// Create a session with a salt (can be any string, but should be unique per user)
const session = beekeeper.createSession("salt_" + Math.random());

// Create a wallet with a specified password. If wallet with the same name exists, the error will be thrown.
const { wallet } = await session.createWallet('wallet0', 'password');

// Import keys into the wallet. The returned value is the public key of the imported private key.
// Keys can be generated using `@hiveio/wax` or any other library that can generate Hive keys.
const publicKey1 = await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
const publicKey2 = await wallet.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

await wallet.removeKey(publicKey1);

console.log(wallet.hasMatchingPrivateKey(publicKey1)); // false

console.log(wallet.getPublicKeys()); // [publicKey2]

// Close the session when done. This will also safely close all wallets created in this session.
session.close();
```

### Listing all wallets

```js
import beekeeperFactory from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

const session = beekeeper.createSession("salt_" + Math.random());

// There should be a wallet created from the previous example
for(const wallet of session.listWallets())
  console.log(`Available wallet: ${wallet.name}`); // Available wallet: wallet0
```

### Retrieving session information

```js
import beekeeperFactory from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory({
  unlockTimeout: 10 // seconds
});

const session = beekeeper.createSession("salt_" + Math.random());

const info = session.getInfo();

// Should be close to 10 seconds
const locksIn = info.timeoutTime.getTime() - Date.now();

console.log(`All wallets will be automatically locked in ${locksIn} ms if not used.`);
```

If you still want to use the wallet after the timeout, you can call `wallet.unlock('your.password')` again.

**Every time you use the wallet, the timeout is reset.**

### Use beekeeper in-memory

```js
import beekeeperFactory from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory({
  inMemory: true // This will make sure that no data is persisted to disk
});

const session = beekeeper.createSession("salt_" + Math.random());

// Note: We are providing `true` as the third argument to create a wallet.
// This means that the wallet will be created in "in-memory" mode, and will not be persisted to disk.
// When using inMemory mode in Beekeeper options, this argument is optional, as all wallets will be created as temporary.
const { wallet } = await session.createWallet('wallet1', 'password', true);

console.log(wallet.isTemporary); // true

// Clean up - Encrypt and remove all data from memory
session.close();
```

### Open wallet if exists

```ts
import beekeeperFactory, { IBeekeeperUnlockedWallet } from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

const session = beekeeper.createSession("salt_" + Math.random());

let unlockedWallet: IBeekeeperUnlockedWallet;

if (session.hasWallet('wallet0')) {
  const lockedWallet = session.openWallet('wallet0');

  unlockedWallet = lockedWallet.unlock('password');
} else {
  const { wallet } = await session.createWallet('wallet0', 'password');

  unlockedWallet = wallet;
}

// Public key from the previous example: [ 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa' ]
console.log(unlockedWallet.getPublicKeys());

// Clean up
session.close();
```

### Sign transaction digest

```js
import beekeeperFactory, { IBeekeeperUnlockedWallet } from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

const session = beekeeper.createSession("salt_" + Math.random());

const unlockedWallet = session.openWallet('wallet0').unlock('password');

// Example transaction digest (can be retrieved from the @hiveio/wax library after creating transaction)
const sigDigest = "f1d3ff8443297732862df21dc4e57262a2b0b6f8c5f9f1d3ff8443297732862d";

// This is the public key we imported into the wallet in previous example
const myPublicKey = "STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa";

// Sign the digest using the private key associated with the public key in the wallet
const signature = unlockedWallet.signDigest(myPublicKey, sigDigest);

// Signature: '1f079d1b5aa5791cc0ea676978e4cf2a5c40ae30c893c9daa5ec01366b4b5a7ef92b1e22df42e294fc8c797f948b966eb21126a229bb992da4142d91212d974ee3'
console.log("Signature:", signature);

// Clean up
session.close();
```

### Encrypt and decrypt message

```js
import beekeeperFactory, { IBeekeeperUnlockedWallet } from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

const session = beekeeper.createSession("salt_" + Math.random());

const { wallet } = await session.createWallet('wallet2', 'password');

const publicKey1 = await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
const publicKey2 = await wallet.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

const message = "Hello, Hive!";

const encrypted = wallet.encryptData(message, publicKey1, publicKey2);

console.log('Encrypted message:', encrypted); // '2TaGvWPe2e92WiQWPVwo38gdtVapZDpYUePLhFRx'

const decrypted = wallet.decryptData(encrypted, publicKey1, publicKey2);

console.log('Decrypted message:', decrypted); // 'Hello, Hive!'

// Clean up
session.close();
```

### Use beekeeper on Vite-bundled project

Some users may want to use beekeeper in a Vite-bundled project, such as Next / Nuxt projects.
And as we are using WebAssembly (WASM) under the hood, we need to ensure that the WASM file is properly loaded.
To do so, we prepared a special import path that points to a Beekeeper build with special Vite `?url` WASM import.

Note: This only applies for browser environments if you are using Vite as your bundler. If you are using Webpack or other bundlers, you can use the default import path.

```js
import beekeeperFactory from '@hiveio/beekeeper/vite';

const beekeeper = await beekeeperFactory();

const session = beekeeper.createSession("my.salt");

const sessionInfo = session.getInfo();

console.log(sessionInfo);
```

## 📖 API Reference

For a detailed API definition, please see our [Wiki](${GEN_DOC_URL}).

## 🛠️ Development and Testing

If you want to use development versions of our packages, set `@hiveio` scope to use our GitLab registry:

```bash
echo @hiveio:registry=https://gitlab.syncad.com/api/v4/groups/136/-/packages/npm/ >> .npmrc
npm install @hiveio/beekeeper
```

### Environment Setup

Clone the repository and install the dependencies:

```bash
git clone https://gitlab.syncad.com/hive/hive.git --recurse-submodules
cd hive/programs/beekeeper/beekeeper_wasm

corepack enable
pnpm install
```

### Build

Compile the TypeScript source code:

```bash
pnpm build
```

### Run Tests

Execute the test suite:

```bash
pnpm test
```

---

## 📄 License

This project is licensed under the MIT License. See the [LICENSE.md](https://gitlab.syncad.com/hive/hive/-/blob/master/LICENSE.md) file for details.
