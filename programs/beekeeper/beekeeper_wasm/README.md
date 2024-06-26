# beekeeper

Hive Beekeeper functionality exposed to TypeScript/JavaScript environments

## Install

This is a [Node.js](https://nodejs.org/en/) module available through the
[npm registry](https://www.npmjs.com/).

Before installing, [download and install Node.js](https://nodejs.org/en/download/).
Node.js 12 or higher is required.

Installation is done using the
[`npm install` command](https://docs.npmjs.com/getting-started/installing-npm-packages-locally):

```bash
npm install @hiveio/beekeeper
```

## Usage

```js
import beekeeperFactory from '@hiveio/beekeeper';

const beekeeper = await beekeeperFactory();

const session = await beekeeper.createSession("my.salt");

const { wallet } = await session.createWallet('w0', 'mypassword');

await wallet.importKey('5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT');
await wallet.importKey('5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78');

await wallet.removeKey('mypassword', '6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa');

console.log(await wallet.getPublicKeys());
```

## API

See API documentation at [project WIKI](${GEN_DOC_URL})

## Support and tests

Tested on the latest Chromium (v117)

[Automated CI test](https://gitlab.syncad.com/hive/hive/-/pipelines) runs are available.

To run the tests on your own, clone the Hive repo and install the dependencies following this documentation:

[Getting Hive source code](https://gitlab.syncad.com/hive/hive/-/blob/master/doc/building.md?ref_type=heads#getting-hive-source-code)

and then compile the project:

```bash
./scripts/build_wasm_beekeeper.sh
cd hive/programs/beekeeper/beekeeper_wasm
sudo npm install -g pnpm
pnpm install --frozen-lockfile
```

Compile source:

```bash
npm run build
```

Then run tests:

```bash
npm run test
```

## License

See license in the [LICENSE.md](https://gitlab.syncad.com/hive/hive/-/blob/master/LICENSE.md?ref_type=heads) file

