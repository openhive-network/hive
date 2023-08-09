import { strict as assert } from 'node:assert';
import path from 'path';

import { setTimeout } from 'timers/promises';
import { fileURLToPath } from 'url';

import Module from '../../../../build_wasm/beekeeper_wasm.mjs';

import BeekeeperInstanceHelper from './run_node_helper.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const wallet_names = ["w0","w1","w2","w3","w4","w5","w6","w7","w8","w9"];
const passwords = new Map();

const keys =
[
  ['5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT', '5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh'],
  ['5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78', '6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa'],
  ['5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS', '7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH'],
  ['5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n', '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4'],
  ['5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn', '6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm'],
  ['5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze', '6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW'],
  ['5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su', '8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME'],
  ['5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw', '8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4'],
  ['5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG', '6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45'],
  ['5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg', '8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5']
];

const sign_data =
[
  {
    'public_key': '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
    'sig_digest': '390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542',
    'binary_transaction_body': '000000000000000000000000',
    'transaction_body': '{}',
    'expected_signature': '1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21'
  },
  {
    'public_key': '6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4',
    'sig_digest': '614e645c13b351b56d9742b358e3c3da58fa1a6a0036a01d3163c21aa2c8a99c',
    'binary_transaction_body': '5f00c58fb5f9854fb664010209696e69746d696e657205616c6963659a020000000000002320bcbe056d656d6d6d00',
    'transaction_body': '{"ref_block_num":95,"ref_block_prefix":4189425605,"expiration":"2023-07-18T08:38:29","operations":[{"type":"transfer_operation","value":{"from":"initminer","to":"alice","amount":{"amount":"666","precision":3,"nai":"@@000000021"},"memo":"memmm"}}],"extensions":[],"signatures":[],"transaction_id":"cc9630cdbc39da1c9b6264df3588c7bedb5762fa","block_num":0,"transaction_num":0}',
    'expected_signature': '1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340'
  },
];

const chain_id = '18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e';

const STORAGE_ROOT = path.join(__dirname, 'storage_root');

const my_entrypoint = async() => {
  const provider = await Module();

  console.log(`Storage root for testing is: ${STORAGE_ROOT}`);

  const args = `--wallet-dir ${STORAGE_ROOT}/.beekeeper --salt avocado`;

  const beekeper = BeekeeperInstanceHelper.for(provider);
  const api = new beekeper(...args.split(' '));

  console.log(api.token);

  process.exit(0);

  const params = perform_StringList_test(provider);

  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Basic tests which execute all endpoints")
    console.log("**************************************************************************************")

    const [ beekeeper_instance, implicit_session_token ] = create_beekeeper_instance(provider, params);

    create(beekeeper_instance, implicit_session_token, 0, '');

    import_key(beekeeper_instance, implicit_session_token, 0, 0);

    let [_session_token, __response_status] = create_session(beekeeper_instance, 'pear');
    close_session(beekeeper_instance, _session_token)

    let [session_token, _response_status] = create_session(beekeeper_instance, 'pear');

    create(beekeeper_instance, session_token, 1, 'cherry');

    create(beekeeper_instance, session_token, 2, '');

    import_key(beekeeper_instance, session_token, 2, 1);
    import_key(beekeeper_instance, session_token, 2, 2);
    import_key(beekeeper_instance, session_token, 2, 3);

    let [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":true}]}');

    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"7j1orEPpWp4bU2SuH46eYXuXkFKEMeJkuXkZVJSaru2zFDGaEH"}]}');

    close(beekeeper_instance, session_token, 2);
    
    [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true}]}');

    open(beekeeper_instance, session_token, 2);

    [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":false}]}');

    unlock(beekeeper_instance, session_token, 2);

    [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":true}]}');

    lock(beekeeper_instance, session_token, 2);

    [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);
    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":true},{"name":"w2","unlocked":false}]}');

    lock_all(beekeeper_instance, session_token);

    [wallets, _response_status_wallets] = list_wallets(beekeeper_instance, session_token);

    assert.equal(wallets, '{"wallets":[{"name":"w1","unlocked":false},{"name":"w2","unlocked":false}]}');

    unlock(beekeeper_instance, session_token, 2);

    remove_key(beekeeper_instance, session_token, 2, 2);

    [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}');

    get_info(beekeeper_instance, session_token);

    unlock(beekeeper_instance, session_token, 1);

    list_wallets(beekeeper_instance, session_token);

    get_public_keys(beekeeper_instance, session_token);

    {
      let [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 0);
      assert.equal(_response_status_sign_trx, true);
    }
    {
      let [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 1);
      assert.equal(_response_status_sign_trx, true);
    }

    await perform_wallet_autolock_test(beekeeper_instance, session_token);

    delete_instance(beekeeper_instance)
  }

  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Trying to open beekeeper instance again so as to find out if data created in previous test is permamently saved")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    beekeeper_instance.list_wallets(session_token);

    create(beekeeper_instance, session_token, 3, '');

    list_wallets(beekeeper_instance, session_token);

    unlock(beekeeper_instance, session_token, 2);

    list_wallets(beekeeper_instance, session_token);

    import_key(beekeeper_instance, session_token, 3, 9);

    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}]}');
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Create 3 wallets and add to every wallet 3 the same keys. Every key should be displayed only once")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    create(beekeeper_instance, session_token, 4, '');
    create(beekeeper_instance, session_token, 5, '');
    create(beekeeper_instance, session_token, 6, '');

    for(let wallet_number = 4; wallet_number <= 6; wallet_number++)
    {
      for(let key_number = 7; key_number <= 9; key_number++)
      {
        import_key(beekeeper_instance, session_token, wallet_number, key_number);
      }
    }
    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"},{"public_key":"8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},{"public_key":"8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}]}');
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Remove all keys from 3 wallets created in previous step. As a result all keys are removed in mentioned wallets.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    unlock(beekeeper_instance, session_token, 4);
    unlock(beekeeper_instance, session_token, 5);
    unlock(beekeeper_instance, session_token, 6);

    for(let wallet_number = 4; wallet_number <= 6; wallet_number++)
    {
      for(let key_number = 7; key_number <= 9; key_number++)
      {
        remove_key(beekeeper_instance, session_token, wallet_number, key_number);
      }
    }
    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[]}');
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Close implicitly created session. Create 3 new sessions. Every session has a 1 wallet. Every wallet has unique 1 key. As a result there are 3 keys.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);
    close_session(beekeeper_instance, session_token);

    let session_tokens = [
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado')
    ]

    let wallet_number = 7;
    let key_number = 4;
    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      create(beekeeper_instance, session_tokens[session_number], wallet_number, '');
      import_key(beekeeper_instance, session_tokens[session_number], wallet_number, key_number);
      wallet_number++;
      key_number++;
    }

    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_tokens[session_number]);
      switch(session_number)
      {
        case 0:
          assert.equal(public_keys, '{"keys":[{"public_key":"6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"}]}');
          break;
        case 1:
          assert.equal(public_keys, '{"keys":[{"public_key":"6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"}]}');
          break;
        case 2:
          assert.equal(public_keys, '{"keys":[{"public_key":"8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"}]}');
          break;
        default:
          assert.equal(false);
      }
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Unlock 10 wallets. From every wallet remove every key.")
    console.log("Removing is done by passing always all public keys from `keys` set. Sometimes `remove` operation passes, sometimes (mostly) fails.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    for(let wallet_number = 0; wallet_number < wallet_names.length; wallet_number++)
    {
      unlock(beekeeper_instance, session_token, wallet_number);
    }
    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[{"public_key":"5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"},{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6Pg5jd1w8rXgGoqvpZXy1tHPdz43itPW6L2AGJuw8kgSAbtsxm"},{"public_key":"6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"},{"public_key":"8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"},{"public_key":"8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5"}]}');

    for(let wallet_number = 0; wallet_number < wallet_names.length; wallet_number++)
    {
      unlock(beekeeper_instance, session_token, wallet_number);
      for(let key_number = 0; key_number < keys.length; key_number++)
      {
        remove_key(beekeeper_instance, session_token, wallet_number, key_number);
      }
    }

    [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[]}');
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Close implicitly created session. Create 4 new sessions. All sessions open the same wallet.")
    console.log("Sessions{0,2} import a key, sessions{1,3} try to remove a key (here is an exception ('Key not in wallet')).")
    console.log("As a result only sessions{0,2} have a key.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    close_session(beekeeper_instance, session_token);

    let session_tokens = [
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado')
    ]

    let public_keys = 0;
    let _response_status_public_keys = 0;

    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      unlock(beekeeper_instance, session_tokens[session_number], 0);
      [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_tokens[session_number]);
      assert.equal(public_keys, '{"keys":[]}');
    }

    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      if( (session_number % 2) == 0 )
      {
        import_key(beekeeper_instance, session_tokens[session_number], 0, 0);
        [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_tokens[session_number]);
        assert.equal(public_keys, '{"keys":[{"public_key":"5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]}');
      }
      else
      {
        remove_key(beekeeper_instance, session_tokens[session_number], 0, 0);
        [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_tokens[session_number]);
        assert.equal(public_keys, '{"keys":[]}');
      }
    }

    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      if( (session_number % 2) == 0 )
      {
        public_keys = get_public_keys(beekeeper_instance, session_tokens[session_number]);
      }
      else
      {
        public_keys = get_public_keys(beekeeper_instance, session_tokens[session_number]);
      }
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Close implicitly created session. Create 4 new sessions. All opened sessions should have the same key created in previous step.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    close_session(beekeeper_instance, session_token);

    let session_tokens = [
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado'),
      create_session_ex(beekeeper_instance, 'avocado')
    ]

    let public_keys = 0;
    let _response_status_public_keys = 0;

    for(let session_number = 0; session_number < session_tokens.length; session_number++)
    {
      unlock(beekeeper_instance, session_tokens[session_number], 0);
      [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_tokens[0]);
      assert.equal(public_keys, '{"keys":[{"public_key":"5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]}');
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("False tests for every API endpoint.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    {
      console.log("==================================:00");
      let [_response_message, _response_status] = close_session(beekeeper_instance, 'this-token-doesnt-exist-00');
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:01");
      let [_response_message, _response_status] = close(beekeeper_instance, 'this-token-doesnt-exist-01', 5);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:02");
      let [_response_message, _response_status] = open(beekeeper_instance, 'this-token-doesnt-exist-02', 6);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:03");
      let [_response_message, _response_status] = create(beekeeper_instance, 'this-token-doesnt-exist-03', 0, '');
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:04");
      let [_response_message, _response_status] = unlock(beekeeper_instance, 'this-token-doesnt-exist-04', 0);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:05");
      let [_response_message, _response_status] = set_timeout(beekeeper_instance, 'this-token-doesnt-exist-05', 1);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:06");
      let [_response_message, _response_status] = lock_all(beekeeper_instance, 'this-token-doesnt-exist-06');
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:07");
      let [_response_message, _response_status] = lock(beekeeper_instance, 'this-token-doesnt-exist-07', 0);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:08");
      let [_response_message, _response_status] = import_key(beekeeper_instance, 'this-token-doesnt-exist-08', 0, 0);
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:09");
      let [_response_message, _response_status] = remove_key(beekeeper_instance, 'this-token-doesnt-exist-09', 0, 0, "public-key-doesnt-exist");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:10");
      let [_response_message, _response_status] = list_wallets(beekeeper_instance, 'this-token-doesnt-exist-10');
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:11");
      let [_response_message, _response_status] = get_public_keys(beekeeper_instance, 'this-token-doesnt-exist-11');
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:12");
      let [_response_message, _response_status] = sign_digest(beekeeper_instance,
        'this-token-doesnt-exist-12',
        "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542",
        "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:13");
      let [_response_message, _response_status] = sign_digest(beekeeper_instance,
        session_token,
        "390f34297cfcb8fa4b37353431ecbab05b8dc0c9c15fb9ca1a3d510c52177542",
        "this is not public key",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:14");
      let [_response_message, _response_status] = sign_digest(beekeeper_instance,
        session_token,
        "this is not digest of transaction",
        "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:15");
      let [_response_message, _response_status] = sign_binary_transaction(beekeeper_instance,
        'this-token-doesnt-exist-13',
        "000000000000000000000000",
        chain_id,
        "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:16");
      let _key_number = 0;
      let _wallet_number = 0;
      let [_response_message_unlock_wallet, _response_status_unlock_wallet] = unlock(beekeeper_instance, session_token, _wallet_number);
      let [_response_message_import_key, _response_status_import_key] = import_key(beekeeper_instance, session_token, _wallet_number, _key_number);
      let [_response_message, _response_status] = sign_binary_transaction(beekeeper_instance,
        session_token,
        "really! it means nothing!!!!! this is not a transaction body",
        chain_id,
        keys[_key_number][1],
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:17");
      let [_response_message, _response_status] = sign_binary_transaction(beekeeper_instance,
        session_token,
        "000000000000000000000000",
        chain_id,
        'this is not public key',
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:18");
      let _key_number = 0;
      let [_response_message, _response_status] = sign_binary_transaction(beekeeper_instance,
        session_token,
        "000000000000000000000000",
        "zzz...",
        keys[_key_number][1],
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:19");
      let [_response_message, _response_status] = sign_transaction(beekeeper_instance,
        'this-token-doesnt-exist-14',
        "{}",
        chain_id,
        "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:20");
      let _key_number = 0;
      let [_response_message, _response_status] = sign_transaction(beekeeper_instance,
        session_token,
        "a body of transaction without any sense",
        chain_id,
        keys[_key_number][1],
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:21");
      let _key_number = 0;
      let [_response_message, _response_status] = sign_transaction(beekeeper_instance,
        session_token,
        "{}",
        "+++++",
        keys[_key_number][1],
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
    {
      console.log("==================================:22");
      let [_response_message, _response_status] = sign_transaction(beekeeper_instance,
        session_token,
        "{}",
        chain_id,
        "&&&&",
        "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21");
      assert.equal( _response_status, false );
    }
  }
  {
    let nr_sessions = 63;//63 because at the beginning we have implicitly created session
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Try to create ${nr_sessions + 2} new sessions, but due to the limit creating the last one fails.`)
    console.log("**************************************************************************************")
    let [ beekeeper_instance, implicitly_created_session_token ] = create_beekeeper_instance(provider, params);

    for(let session_number = 0; session_number < nr_sessions; session_number++)
    {
      let [_session_token, _response_status]  = create_session(beekeeper_instance, "xyz");
      assert.equal( _response_status, true );
    }
    let [_session_token, _response_status]  = create_session(beekeeper_instance, "xyz2");
    assert.equal( _response_status, false );
  }
  {
    let nr_sessions = 64;
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Close implicitly created session. Create ${nr_sessions} new sessions, unlock 10 wallets and add 10 keys to every wallet.`)
    console.log("Destroy beekeeper without sessions' closing.")
    console.log("**************************************************************************************")
    let [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    close_session(beekeeper_instance, session_token);

    let nr_wallets = 10;
    let nr_keys = 10;

    for(let session_number = 0; session_number < nr_sessions; session_number++)
    {
      session_token = create_session_ex(beekeeper_instance, session_number.toString());
      for( let wallet_number = 0; wallet_number < nr_wallets; wallet_number++ )
      {
        let [_response_message_unlock, _response_status_unlock] = unlock(beekeeper_instance, session_token, wallet_number);
        assert.equal(_response_status_unlock, true);
        for( let key_number = 0; key_number < nr_keys; key_number++ )
        {
          import_key(beekeeper_instance, session_token, wallet_number, key_number);
        }
      }
    }
    delete_instance(beekeeper_instance);
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Remove every key from every wallet using implicitly created session. Each removal passes.`)
    console.log(`Finally there isn't any key in unlocked wallets.`)
    console.log("**************************************************************************************")
    let [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    let nr_wallets = 10;
    let nr_keys = 10;

    for( let wallet_number = 0; wallet_number < nr_wallets; wallet_number++ )
    {
      let [_response_message_unlock, _response_status_unlock] = unlock(beekeeper_instance, session_token, wallet_number);
      assert.equal(_response_status_unlock, true);
      for( let key_number = 0; key_number < nr_keys; key_number++ )
      {
        let [_response_message_import_key, _response_status_import_key] = remove_key(beekeeper_instance, session_token, wallet_number, key_number);
        assert.equal(_response_status_import_key, true);
      }
    }
    let [public_keys, _response_status_public_keys] = get_public_keys(beekeeper_instance, session_token);
    assert.equal(public_keys, '{"keys":[]}');
    assert.equal(_response_status_public_keys, true);
  }
  {
    let nr_sessions = 64;
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Create ${nr_sessions - 1} new sessions, close them + implicitly created session. Create again ${nr_sessions} and close them.`)
    console.log("**************************************************************************************")
    let [ beekeeper_instance, implicitly_created_session_token ] = create_beekeeper_instance(provider, params);

    let session_tokens = []

    for(let session_number = 0; session_number < nr_sessions - 1; session_number++)
    {
      let [_response_message_create_session, _response_status_create_session] = create_session(beekeeper_instance, session_number.toString())
      assert.equal(_response_status_create_session, true);
      session_tokens.push( _response_message_create_session );
    }

    for(let session_number = 0; session_number < nr_sessions - 1; session_number++)
    {
      let [_response_message_close_session, _response_status_close_session] = close_session(beekeeper_instance, session_tokens[session_number]);
      assert.equal(_response_status_close_session, true);
    }

    {
      let [_response_message_close_session, _response_status_close_session] = close_session(beekeeper_instance, implicitly_created_session_token);
      assert.equal(_response_status_close_session, true);
    }

    session_tokens = [];

    for(let session_number = 0; session_number < nr_sessions; session_number++)
    {
      let [_response_message_create_session, _response_status_create_session] = create_session(beekeeper_instance, session_number.toString())
      assert.equal(_response_status_create_session, true);
      session_tokens.push( _response_message_create_session );
    }

    for(let session_number = 0; session_number < nr_sessions; session_number++)
    {
      let [_response_message_close_session, _response_status_close_session] = close_session(beekeeper_instance, session_tokens[session_number]);
      assert.equal(_response_status_close_session, true);
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Try to sign transactions, but they fail. Unlock wallet. Import key. Sign transactions.`);
    console.log(`Delete instance. Create instance again.`);
    console.log(`Unlock wallet. Sign transactions. Remove key. Try to sign transactions again, but they fail.`);
    console.log("**************************************************************************************")
    let [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    let _response_message_sign_trx = 0;
    let _response_status_sign_trx = 0;

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 0);
    assert.equal(_response_status_sign_trx, false);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 1);
    assert.equal(_response_status_sign_trx, false);

    let wallet_number = 7;
    let key_number = 3;

    unlock(beekeeper_instance, session_token, wallet_number);
    import_key(beekeeper_instance, session_token, wallet_number, key_number);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 0);
    assert.equal(_response_status_sign_trx, true);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 1);
    assert.equal(_response_status_sign_trx, true);

    delete_instance(beekeeper_instance);

    [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    unlock(beekeeper_instance, session_token, wallet_number);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 0);
    assert.equal(_response_status_sign_trx, true);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 1);
    assert.equal(_response_status_sign_trx, true);

    remove_key(beekeeper_instance, session_token, wallet_number, key_number);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 0);
    assert.equal(_response_status_sign_trx, false);

    [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_token, 1);
    assert.equal(_response_status_sign_trx, false);
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Unlock 10 wallets. Every wallet has own session. Import the same key into every wallet. Sign transactions using the key from every wallet.`);
    console.log(`Lock even wallets. Sign transactions using the key from: every odd wallet (passes), every even wallet (fails).`);
    console.log(`Lock odd wallets. Sign transactions using the key from every wallet (always fails).`);
    console.log("**************************************************************************************")
    let [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    let nr_wallets = 10;
    let key_number = 3;

    let session_tokens = []

    for(let session_number = 0; session_number < nr_wallets; session_number++)
      session_tokens.push( create_session_ex(beekeeper_instance, session_number.toString()) );

    for(let wallet_number = 0; wallet_number < nr_wallets; wallet_number++)
    {
      let [_response_message_unlock, _response_status_unlock] = unlock(beekeeper_instance, session_tokens[wallet_number], wallet_number);
      assert.equal(_response_status_unlock, true);

      let [_response_message_import_key, _response_status_import_key] = import_key(beekeeper_instance, session_tokens[wallet_number], wallet_number, key_number);
      assert.equal(_response_status_import_key, true);

      let [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_tokens[wallet_number], 1);
      assert.equal(_response_status_sign_trx, true);
    }

    for(let wallet_number = 0; wallet_number < nr_wallets; wallet_number+=2)
    {
      let [_response_message_lock, _response_status_lock] = lock(beekeeper_instance, session_tokens[wallet_number], wallet_number);
      assert.equal(_response_status_lock, true);
    }

    for(let wallet_number = 0; wallet_number < nr_wallets; wallet_number++)
    {
      let [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_tokens[wallet_number], 1);
      assert.equal(_response_status_sign_trx, ( wallet_number % 2 ) == 1 );
    }

    for(let wallet_number = 0; wallet_number < nr_wallets; wallet_number+=2)
    {
      let [_response_message_lock, _response_status_lock] = lock(beekeeper_instance, session_tokens[wallet_number+1], wallet_number+1);
      assert.equal(_response_status_lock, true);
    }

    for(let wallet_number = 0; wallet_number < nr_wallets; wallet_number++)
    {
      let [_response_message_sign_trx, _response_status_sign_trx] = sign_transaction_using_3_methods(beekeeper_instance, session_tokens[wallet_number], 1);
      assert.equal(_response_status_sign_trx, false);
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log(`Check endpoints related to timeout: "set_timeout", "get_info",`);
    console.log("**************************************************************************************")
    let [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);
    {
      let [_now, _timeout_time, _response_status_get_info] = get_info(beekeeper_instance, session_token);
      assert.equal(_response_status_get_info, true);
    }
    {
      let [_response_message_set_timeout, _response_status_set_timeout] = set_timeout(beekeeper_instance, session_token, 3600);
      assert.equal(_response_status_set_timeout, true);
    }
    {
      let [_now, _timeout_time, _response_status_get_info] = get_info(beekeeper_instance, session_token);
      assert.equal(_response_status_get_info, true);
      assert.equal(_timeout_time - _now, 3600 * 1000);
    }
    {
      //Zero is allowed.
      let [_response_message_set_timeout, _response_status_set_timeout] = set_timeout(beekeeper_instance, session_token, 0);
      assert.equal(_response_status_set_timeout, true);
    }
    {
      //Text implictly converted to zero.
      let [_response_message_set_timeout, _response_status_set_timeout] = set_timeout(beekeeper_instance, session_token, `this is not a number of seconds`);
      assert.equal(_response_status_set_timeout, true);
    }
    {
      let [_now, _timeout_time, _response_status_get_info] = get_info(beekeeper_instance, session_token);
      assert.equal(_response_status_get_info, true);
      assert.equal(_timeout_time - _now, 0);
    }
  }
  {
    console.log("")
    console.log("**************************************************************************************")
    console.log("Different false tests for 'sign*' endpoints.")
    console.log("**************************************************************************************")
    const [ beekeeper_instance, session_token ] = create_beekeeper_instance(provider, params);

    let wallet_number = 9;

    let [_response_message_unlock, _response_status_unlock] = unlock(beekeeper_instance, session_token, wallet_number);
    assert.equal(_response_status_unlock, true);

    {
     let _length = 10000000;//10 mln
     let _long_string = 'a'.repeat(_length)
     let [_response_message, _response_status] = sign_digest(beekeeper_instance,
        session_token,
        _long_string,
        sign_data[1].public_key,
        sign_data[1].expected_signature);
      assert.equal( _response_status, false );
    }
    {
      let _length = 50;
      let _long_string = '9'.repeat(_length)

      {
        let [_response_message_sign_digest, _response_status_sign_digest] = sign_binary_transaction(beekeeper_instance,
          session_token,
          _long_string,
          chain_id,
          sign_data[1].public_key,
          sign_data[1].expected_signature);
        assert.equal( _response_status_sign_digest, false );
      }
      {
        let [_response_message_sign_digest, _response_status_sign_digest] = sign_transaction(beekeeper_instance,
          session_token,
          _long_string,
          chain_id,
          sign_data[1].public_key,
          sign_data[1].expected_signature);
        assert.equal( _response_status_sign_digest, false );
      }
    }
  }

  console.log('##########################################################################################');
  console.log('##                             ALL TESTS PASSED                                         ##');
  console.log('##########################################################################################');
};

my_entrypoint()
  .then(() => process.exit(0))
  .catch(err => {
    console.error(err);
    process.exit(1);
  });
