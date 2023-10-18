# beekeeper

call hived functions from JavaScript

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

const sessionData = await beekeeper.create_session('pear');

console.log(beekeeper.get_info(sessionData.token));
```

## API

**(WIP)**

## Support and tests

Tested on the latest Chromium (v117)

[Automated CI test](https://gitlab.syncad.com/hive/hive/-/pipelines) runs are available.

To run the tests on your own, clone the Hive repo and install the dependencies:

```bash
git clone https://gitlab.syncad.com/hive/hive.git --depth 1
cd hive/programs/beekeeper/beekeeper_wasm
sudo npm install -g pnpm
pnpm install
```

Then run tests:

```bash
npm run test
```

## License

See license in the [LICENSE.md](LICENSE.md) file
