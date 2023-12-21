import { test, expect } from '@playwright/test';

import type beekeepermodule from '../../dist/beekeeper.node.js';
import BeekeeperModule from '../../build/beekeeper_wasm.node.js';
import beekeeperFactory from '../../dist/bundle/node.js';
import { ExtractError, BeekeeperInstanceHelper } from '../assets/run_node_helper.js';

import { WALLET_OPTIONS_NODE } from '../assets/data.js';

test.describe('WASM Base tests for Node.js', () => {
  test('Should have global module', async () => {
    expect(typeof BeekeeperModule).toBe('function');
  });

  test('Should be able to create instance of StringList', async () => {
    const provider = await (BeekeeperModule as typeof beekeepermodule)();

    new provider.StringList();
  });

  test('Should be able to create instance of beekeeper_api', async () => {
    const provider = await (BeekeeperModule as typeof beekeepermodule)();
    const beekeeperOptions = new provider.StringList();
    WALLET_OPTIONS_NODE.forEach((opt) => void beekeeperOptions.push_back(opt));

    new provider.beekeeper_api(beekeeperOptions);
  });

  test('Should be able to create instance of ExtractError - import script', async () => {
    new ExtractError("");
  });

  test('Should be able to create instance of BeekeeperInstanceHelper', async () => {
    const provider = await (BeekeeperModule as typeof beekeepermodule)();

    new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE)
  });

  test('Should be able to create instance of beekeeper factory', async () => {
    await beekeeperFactory();
  });
});
