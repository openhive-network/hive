# TypeScript beekeeper

## Configure

**It is strongly advised to use our development image: `registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:3.1.43`**

If you want to test our package locally follow those steps:

First install the package manager:

```bash
sudo npm i -g pnpm
```

And all of the required dependencies:

```bash
pnpm install
```

## Build

```bash
npm run build
```

## Test

We use playwright in our tests to emulate WASM behaviour in the browser environment:

```bash
npm run test
```

## License

See license in the [LICENSE.md](../../../LICENSE.md) file
